package com.opentail.android;

import org.junit.Test;
import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;

import static org.junit.Assert.*;

/** Untrusted Android document reads must be bounded before v4 JSON parsing. */
public class NativeProjectDocumentIoTest {
    @Test public void acceptsExactImportLimitAndPreservesCharacters() throws Exception {
        String exact = "a".repeat(NativeProjectDocumentIo.MAX_CHARS);
        assertEquals(exact,
            NativeProjectDocumentIo.readBounded(new StringReader(exact)));
    }

    @Test public void rejectsOneCharacterOverTheLimitWithoutReturningPartialData() {
        String tooBig = "a".repeat(NativeProjectDocumentIo.MAX_CHARS + 1);
        assertThrows(IOException.class,
            () -> NativeProjectDocumentIo.readBounded(new StringReader(tooBig)));
    }

    @Test public void propagatesRealDocumentIoErrors() {
        Reader broken = new Reader() {
            @Override public int read(char[] chars, int off, int len) throws IOException {
                throw new IOException("Document provider failed");
            }
            @Override public void close() {}
        };
        IOException failure = assertThrows(IOException.class,
            () -> NativeProjectDocumentIo.readBounded(broken));
        assertEquals("Document provider failed", failure.getMessage());
    }
}
