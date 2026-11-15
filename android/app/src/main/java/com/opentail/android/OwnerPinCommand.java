package com.opentail.android;

/** Owner-authenticated enrollment characteristic v3. Skip sends no command. */
final class OwnerPinCommand {
    private OwnerPinCommand() {}
    static byte[] encode(String pin) {
        if(pin==null||!pin.matches("[0-9]{6}"))return null;
        int value=Integer.parseInt(pin);
        return new byte[]{3,1,(byte)value,(byte)(value>>>8),(byte)(value>>>16),(byte)(value>>>24)};
    }
}
