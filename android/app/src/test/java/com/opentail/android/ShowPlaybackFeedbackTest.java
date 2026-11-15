package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

public class ShowPlaybackFeedbackTest {
    private static byte[] wire(int opcode){return new byte[]{1,(byte)opcode};}
    @Test public void classifiesActualControllerResultWithoutClaimingLightOutput(){
        assertEquals("CORE accepted Show effect; verify physical LEDs",
            ShowPlaybackFeedback.describe(wire(10),"OK FX"));
        assertEquals("CORE acknowledged STOP",
            ShowPlaybackFeedback.describe(wire(3),"OK STOP"));
        assertTrue(ShowPlaybackFeedback.describe(wire(10),"ERR BUSY").contains("nonempty Layout zone"));
        assertTrue(ShowPlaybackFeedback.describe(wire(10),"ERR AUTH").contains("owner authorization"));
        assertTrue(ShowPlaybackFeedback.describe(wire(11),"ERR FORMAT").contains("invalid"));
        assertTrue(ShowPlaybackFeedback.describe(wire(10),"GATT SENT").contains("waiting"));
        assertTrue(ShowPlaybackFeedback.describe(wire(3),"ERR TIMEOUT").contains("not confirmed"));
    }
    @Test public void neverReportsOwnerSaveOrUnrelatedResultAsShow(){
        assertNull(ShowPlaybackFeedback.describe(wire(1),"OK AUTH"));
        assertNull(ShowPlaybackFeedback.describe(wire(6),"OK SAVED"));
        assertNull(ShowPlaybackFeedback.describe(new byte[]{2,1},"OK FX"));
        assertNull(ShowPlaybackFeedback.describe(null,"OK FX"));
        assertNotNull(ShowPlaybackFeedback.describe(wire(2),"OK SHOW"));
        assertNotNull(ShowPlaybackFeedback.describe(wire(5),"OK FX"));
        assertNotNull(ShowPlaybackFeedback.describe(wire(11),"OK FX"));
    }
    @Test public void rejectedOrQueuedAreNeverSuccess(){
        assertFalse(ShowPlaybackFeedback.describe(wire(10),"ERR BUSY").contains("accepted"));
        assertFalse(ShowPlaybackFeedback.describe(wire(10),"GATT SENT").contains("accepted"));
        assertFalse(ShowPlaybackFeedback.describe(wire(10),"ERR REPLY READ").contains("accepted"));
    }
}
