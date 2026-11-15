package com.opentail.android;

/** Pure, testable display policy for an authenticated Show/STOP CORE reply.
 *  The GATT write acknowledgement alone is never physical playback proof.
 */
final class ShowPlaybackFeedback {
    private ShowPlaybackFeedback() {}
    static boolean isShowCommand(byte[] wire) {
        if(wire==null || wire.length<2 || wire[0]!=1)return false;
        int op=wire[1]&255;
        return op==2 || op==3 || op==5 || op==10 || op==11;
    }
    static boolean isStop(byte[] wire) {
        return isShowCommand(wire) && (wire[1]&255)==3;
    }
    static String describe(byte[] wire,String reply) {
        if(!isShowCommand(wire))return null; // Never conflate owner or Save replies with Show.
        boolean stop=isStop(wire);
        String action=stop?"STOP":"Show";
        if("GATT SENT".equals(reply))
            return action+" sent over BLE; waiting for CORE reply";
        if("OK STOP".equals(reply) && stop)
            return "CORE acknowledged STOP";
        if(("OK FX".equals(reply)||"OK SHOW".equals(reply)) && !stop)
            return "CORE accepted Show effect; verify physical LEDs";
        if("ERR AUTH".equals(reply))
            return action+" rejected: owner authorization required";
        if("ERR BUSY".equals(reply))
            return action+" rejected (BUSY): verify Output, LED count and a nonempty Layout zone";
        if("ERR RANGE".equals(reply)||"ERR FORMAT".equals(reply))
            return action+" rejected ("+reply+"): effect, palette or target invalid";
        if("ERR TIMEOUT".equals(reply)||"ERR REPLY READ".equals(reply)||"ERR READ".equals(reply)||
            "ERR REPLY MISSING".equals(reply)||"ERR GATT WRITE".equals(reply)||
            "ERR WRITE START".equals(reply))
            return action+" outcome not confirmed ("+reply+"); check BLE connection";
        if("OK RELEASED".equals(reply) && stop)
            return "CORE temporary playback released";
        return action+" controller reply: "+reply;
    }
}
