package com.opentail.android;

/** Local phone-preview clock, independent of BLE/CORE and real GPIO.
 * Timestamp advances once per accepted vsync; recomposition cannot move FX phase.
 * An output uses its own instance so O1/O2 timing never aliases.
 */
final class PreviewFrameClock {
    static final class Sample {
        final long elapsedMs;
        final int frameMs;
        final long skippedIntervals;
        final boolean advance;
        Sample(long elapsedMs,int frameMs,long missed,boolean advance){
            this.elapsedMs=elapsedMs;this.frameMs=frameMs;
            this.skippedIntervals=missed;this.advance=advance;
        }
    }
    private final long nominalNanos;
    private long previous=-1L,elapsed=0L,missed=0L;
    private int accepted=0;
    PreviewFrameClock(){this(60);}
    PreviewFrameClock(int targetFps) {
        if(targetFps<1||targetFps>240)throw new IllegalArgumentException("fps");
        nominalNanos=1_000_000_000L/targetFps;
    }
    Sample onFrame(long nanos) {
        if(nanos<0)throw new IllegalArgumentException("Negative frame timestamp");
        if(previous<0){
            previous=nanos;accepted++;
            return new Sample(elapsed/1_000_000L,16,0,true);
        }
        if(nanos<=previous)return new Sample(elapsed/1_000_000L,16,0,false);
        long delta=nanos-previous;previous=nanos;
        elapsed=Long.MAX_VALUE-elapsed<delta?Long.MAX_VALUE:elapsed+delta;
        final long intervals=(delta+nominalNanos/2)/nominalNanos;
        long dropped=Math.max(0L,intervals-1L);
        missed+=dropped;accepted++;
        int frameMs=(int)Math.min(65535L,Math.max(1L,(delta+500_000L)/1_000_000L));
        return new Sample(elapsed/1_000_000L,frameMs,dropped,true);
    }
    /** Freeze the active local phase across pause/background without counting
     * wall-clock time as renderer time or inventing skipped output frames. */
    void suspendTimestamp(){previous=-1L;}
    int acceptedFrames(){return accepted;}
    long skippedFrames(){return missed;}
}
