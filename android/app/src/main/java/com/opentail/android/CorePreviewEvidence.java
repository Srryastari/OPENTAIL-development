package com.opentail.android;

/** Truthful telemetry status; a valid black frame is still a completed frame.
 * Does not change the approved capsule rendering or any hardware state. */
final class CorePreviewEvidence {
    private CorePreviewEvidence() {}
    static String label(ExactLedFrame frame, String captureStatus, boolean showingFallback) {
        if (frame != null && frame.origin == ExactLedFrame.Origin.CORE) {
            boolean lit=false;
            for (int i=0;i<frame.size();i++) if ((frame.colorAt(i)&0xffffff)!=0) {lit=true;break;}
            if (!lit) return showingFallback ? "CORE frame black · design colors" :
                "CORE frame · "+frame.size()+" LEDs · all black";
            return "CORE frame · "+frame.size()+" LEDs";
        }
        String status=captureStatus == null || captureStatus.isEmpty() ?
            "Awaiting CORE frame" : captureStatus;
        return showingFallback ? status+" · design colors" : status;
    }
}
