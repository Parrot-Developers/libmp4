package com.parrot.libmp4;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.annotation.NonNull;
import java.io.Closeable;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

public class Libmp4 implements Closeable {
    public static final String THERMAL_METADATA_METAVERSION = "com.parrot.thermal.metaversion";
    public static final String THERMAL_METADATA_ALIGNMENT = "com.parrot.thermal.alignment";

    private static final String LIBRARY_NAME = "mp4_android";
    private long demux;

    static {
        System.loadLibrary(LIBRARY_NAME);
    }

    public Libmp4(String fileName) throws IOException {
        demux = nativeOpen(fileName);
        if (demux == 0) {
            throw new IOException("cannot open file " + fileName);
        }
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @NonNull
    public Map<String, String> getMetadata() {
        if (demux != 0) {
            Map<byte[], byte[]> byteMap = nativeGetMetadata(demux);
            if (byteMap == null) {
                return new HashMap<>();
            } else {
                Map<String, String> stringMap = new HashMap<>();
                for (Map.Entry<byte[], byte[]> entry : byteMap.entrySet()) {
                    String key = new String(entry.getKey(), StandardCharsets.ISO_8859_1);
                    String val = new String(entry.getValue(), StandardCharsets.ISO_8859_1);
                    stringMap.put(key, val);
                }
                return stringMap;
            }
        } else {
            return new HashMap<>();
        }
    }

    @Override
    public void close() throws IOException {
        nativeClose(demux);
        demux = 0;
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        if (demux != 0) {
            close();
        }
    }

    private native long nativeOpen(String fileName);
    private native int nativeClose(long demux);
    private native Map<byte[], byte[]> nativeGetMetadata(long demux);
}
