package com.opentail.android;

/**
 * Monitors the authenticated Panel status-v2 sequence diagnostics.
 * A warning is latched locally until an explicit authoritative readback is
 * reconciled; a hard firmware sync fault requires a fresh session.
 */
final class TelemetrySequenceWatchdog {
    static final class Decision {
        final boolean warning,hardFault,requiresReconciliation;
        final String message;
        final PanelPreviewProtocol.Status status;
        Decision(boolean warning,boolean hardFault,boolean reconcile,String message,
                 PanelPreviewProtocol.Status status) {
            this.warning=warning;this.hardFault=hardFault;
            this.requiresReconciliation=reconcile;this.message=message;this.status=status;
        }
    }

    private PanelPreviewProtocol.Status last;
    private boolean warning,hardFault;
    private long hardFaultLane=-1,hardFaultSession=-1;
    private String message;

    synchronized Decision observe(PanelPreviewProtocol.Status current) {
        if(current==null) return decision(null);
        if(last==null) {
            last=current;
            if(current.warningObserved||current.hardSyncFault) {
                warning=true;hardFault=current.hardSyncFault;
                if(hardFault){hardFaultLane=current.lane;hardFaultSession=current.sessionId;}
                message=reason(current);
            }
            return decision(current);
        }

        boolean sameSession=PanelPreviewProtocol.sameStatusIdentity(last,current);
        if(!sameSession) {
            if(last.state==PanelPreviewProtocol.STATE_ACTIVE ||
               last.state==PanelPreviewProtocol.STATE_COMMITTING) {
                warning=true;message="Panel transaction identity changed before the previous session was reconciled";
            }
            // Firmware resets per-session counters at BEGIN. Do not compare
            // those counters across different lane/session identities.
            last=current;
            if(current.hardSyncFault) {
                warning=true;hardFault=true;hardFaultLane=current.lane;
                hardFaultSession=current.sessionId;message=reason(current);
            }
            return decision(current);
        }

        if(current.lastAcceptedSequence!=last.lastAcceptedSequence) {
            long expected=next(last.lastAcceptedSequence);
            if(current.lastAcceptedSequence!=expected) {
                long distance=forwardDistance(expected,current.lastAcceptedSequence);
                if(distance>0 && distance<=0x7fffffffL) {
                    long lastMissing=previous(current.lastAcceptedSequence);
                    String range;
                    if(current.lastAcceptedSequence>expected) {
                        range=expected==lastMissing?Long.toString(expected):
                            expected+"–"+lastMissing;
                    } else if(current.lastAcceptedSequence==1L) {
                        range=expected+"–4294967295 (wrap)";
                    } else {
                        range=expected+"–4294967295, 1–"+lastMissing+" (wrap)";
                    }
                    latchHard("Accepted geometry sequence gap; missing sequence "+range);
                } else {
                    latchHard("Accepted geometry sequence moved backwards or violated wrap ordering");
                }
            }
        }
        if(current.missingCount>last.missingCount)
            latchHard("CORE detected missing geometry UPDATE sequence numbers");
        if(current.outOfOrderCount>last.outOfOrderCount)
            latchHard("CORE detected an out-of-order geometry UPDATE");
        if(current.conflictCount>last.conflictCount)
            latchHard("CORE detected a conflicting duplicate geometry UPDATE");
        if(current.duplicateCount>last.duplicateCount) {
            warning=true;
            if(message==null)message="Duplicate geometry UPDATE observed; verify accepted sequence and candidate CRC";
        }
        if(current.committingRejectCount>last.committingRejectCount ||
           (current.lastRejectedReason==PanelPreviewProtocol.ERR_COMMITTING_LOCKED &&
            current.lastRejectedReason!=last.lastRejectedReason)) {
            warning=true;
            if(message==null)message="A geometry UPDATE was rejected while COMMITTING; waiting for the active commit";
        }
        if(current.hardSyncFault)
            latchHard(reason(current));
        else if(current.lastRejectedReason!=0 &&
                (current.lastRejectedReason!=last.lastRejectedReason ||
                 current.lastRejectedSequence!=last.lastRejectedSequence)) {
            warning=true;
            if(message==null)message=reason(current);
        }
        last=current;
        return decision(current);
    }

    synchronized Decision markDisconnected() {
        warning=true;
        if(last!=null && last.state==PanelPreviewProtocol.STATE_ACTIVE && last.sessionId!=0) {
            hardFault=true;hardFaultLane=last.lane;hardFaultSession=last.sessionId;
            message="BLE disconnected during an active geometry preview; cancel and reload committed geometry";
        } else if(last!=null && last.state==PanelPreviewProtocol.STATE_COMMITTING) {
            message="BLE disconnected during COMMITTING; poll CORE until the durable transaction is terminal";
        } else if(message==null) {
            message="BLE disconnected during a Panel transaction; authoritative reconciliation required";
        }
        return decision(last);
    }

    synchronized Decision markMalformedTelemetry(String why) {
        warning=true;
        message=why==null?"Malformed Panel telemetry; authoritative reconciliation required":why;
        if(last!=null && last.state==PanelPreviewProtocol.STATE_ACTIVE && last.sessionId!=0) {
            hardFault=true;hardFaultLane=last.lane;hardFaultSession=last.sessionId;
        }
        return decision(last);
    }

    synchronized Decision markReconciliationFailed(String why) {
        warning=true;
        message=why==null?"Authoritative geometry reconciliation did not match CORE status":why;
        if(last!=null && last.state==PanelPreviewProtocol.STATE_ACTIVE && last.sessionId!=0) {
            hardFault=true;hardFaultLane=last.lane;hardFaultSession=last.sessionId;
        }
        return decision(last);
    }

    /**
     * Clear only after a stable, verified authoritative map/status read.
     * Hard faults cannot be cleared on the same session that latched them.
     */
    synchronized boolean reconcile(PanelPreviewProtocol.Status authoritative,
                                    boolean mapReadbackVerified) {
        if(authoritative==null||!mapReadbackVerified)return false;
        if(last==null || !PanelPreviewProtocol.sameStatusIdentity(last,authoritative))return false;
        if(authoritative.state==PanelPreviewProtocol.STATE_COMMITTING)return false;
        if(authoritative.hardSyncFault)return false;
        if(hardFault && authoritative.lane==hardFaultLane &&
           authoritative.sessionId==hardFaultSession)return false;
        if(authoritative.activeRevision!=authoritative.renderRevision) {
            warning=true;
            if(message==null)message="CORE render revision has not caught up with the authoritative geometry";
            return false;
        }
        if(authoritative.state==PanelPreviewProtocol.STATE_ACTIVE &&
           authoritative.lastAcceptedSequence!=last.lastAcceptedSequence)return false;
        last=authoritative;warning=false;hardFault=false;message=null;
        hardFaultLane=-1;hardFaultSession=-1;
        return true;
    }

    synchronized boolean requiresCancelFor(PanelPreviewProtocol.Status current) {
        return hardFault && current!=null &&
            current.state==PanelPreviewProtocol.STATE_ACTIVE && current.sessionId!=0 &&
            current.lane==hardFaultLane && current.sessionId==hardFaultSession;
    }

    synchronized boolean awaitingFreshSession(PanelPreviewProtocol.Status current) {
        return hardFault && current!=null &&
            current.state==PanelPreviewProtocol.STATE_CANCELLED &&
            current.lane==hardFaultLane && current.sessionId==hardFaultSession;
    }

    synchronized Decision snapshot() { return decision(last); }
    synchronized boolean hasSession() { return last!=null && last.sessionId!=0; }

    private void latchHard(String why) {
        warning=true;hardFault=true;message=why;
        if(last!=null){hardFaultLane=last.lane;hardFaultSession=last.sessionId;}
    }
    private Decision decision(PanelPreviewProtocol.Status s) {
        return new Decision(warning,hardFault,warning,s==null?message:message,s);
    }
    private static long next(long seq) { return seq==0xffffffffL?1L:seq+1L; }
    private static long previous(long seq) { return seq==1L?0xffffffffL:seq-1L; }
    private static long forwardDistance(long from,long to) {
        return to>=from?to-from:(0xffffffffL-from)+to;
    }
    private static String reason(PanelPreviewProtocol.Status s) {
        if(s==null)return "Panel telemetry unavailable";
        switch(s.lastRejectedReason) {
            case PanelPreviewProtocol.ERR_COMMITTING_LOCKED:
                return "A geometry UPDATE was rejected while COMMITTING; waiting for the active commit";
            case PanelPreviewProtocol.ERR_SEQUENCE_GAP:
                return "CORE detected a missing geometry UPDATE sequence";
            case PanelPreviewProtocol.ERR_SEQUENCE_OUT_OF_ORDER:
                return "CORE detected an out-of-order geometry UPDATE";
            case PanelPreviewProtocol.ERR_SEQUENCE_DUPLICATE:
                return "Duplicate geometry UPDATE observed; verify accepted sequence and candidate CRC";
            case PanelPreviewProtocol.ERR_SEQUENCE_CONFLICT:
                return "CORE detected conflicting payloads using the same geometry sequence";
            case PanelPreviewProtocol.ERR_SYNC_REQUIRED:
                return "CORE requires cancellation and authoritative geometry reload before another edit";
            default:
                return s.hardSyncFault?"CORE geometry synchronization fault is latched":
                       "CORE reported a geometry synchronization warning";
        }
    }
}
