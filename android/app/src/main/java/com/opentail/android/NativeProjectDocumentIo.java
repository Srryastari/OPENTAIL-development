package com.opentail.android;

import java.io.IOException;
import java.io.Reader;

/** Bounded local v4 design import; never read an untrusted document in full
 * before applying the same character limit as NativeProject.importJson(). */
final class NativeProjectDocumentIo {
    static final int MAX_CHARS = 2_000_000;

    static String readBounded(Reader input) throws IOException {
        StringBuilder text = new StringBuilder();
        char[] chunk = new char[8192];
        int count;
        while ((count = input.read(chunk)) != -1) {
            if (count > MAX_CHARS - text.length())
                throw new IOException("Project exceeds 2,000,000 characters");
            text.append(chunk, 0, count);
        }
        return text.toString();
    }

    private NativeProjectDocumentIo() {}
}
