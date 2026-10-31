package com.opentail.android;

/** Local-only JNI binding to the byte-preserved Effects Team renderer.
 * No BLE/hardware paths. Instance state survives parameter edits and reconnects.
 * Explicit close on Compose disposal frees the source engine's scratch buffers.
 */
final class OfflineCoreRenderer implements AutoCloseable {
    static { System.loadLibrary("opentail_preview"); }
    private long handle;
    OfflineCoreRenderer(int effect,int pixels){handle=create(effect,pixels);}
    synchronized boolean available(){return handle!=0;}
    synchronized boolean render(long ms,int frameMs,int speed,int intensity,int brightness,
        int palette,int foreground,int background,byte[] custom,
        float peakHz,float magnitude,boolean audioPresent,int[] out){
        return handle!=0 && frame(handle,ms,frameMs,speed,intensity,brightness,
            palette,foreground,background,custom,peakHz,magnitude,audioPresent,out);
    }
    @Override public synchronized void close(){if(handle!=0){destroy(handle);handle=0;}}
    private static native long create(int effect,int pixels);
    private static native void destroy(long handle);
    private static native boolean frame(long handle,long ms,int frameMs,int speed,
        int intensity,int brightness,int palette,int foreground,int background,
        byte[] custom,float peakHz,float magnitude,boolean audioPresent,int[] out);
}
