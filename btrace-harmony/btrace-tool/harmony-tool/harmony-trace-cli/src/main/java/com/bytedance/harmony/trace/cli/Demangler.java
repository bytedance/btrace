/*
 * Copyright (c) 2026 Bytedance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.bytedance.harmony.trace.cli;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class Demangler {
    private static final boolean SUCCESS;

    static {
        boolean success = false;
        try {
            loadLibrary();
            success = true;
        } catch (Throwable ignore) {
        }
        SUCCESS = success;
    }

    public static void main(String[] args) {
        System.out.println(demangle("_ZN6icu_7212RegexCompile7compileEP5UTextR11UParseErrorR10UErrorCode"));
    }

    private static void loadLibrary() {
        String os = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

        String platform;
        if (os.contains("mac")) {
            platform = "macos";
        } else if (os.contains("nux")) {
            platform = "linux";
        } else {
            throw new RuntimeException("Unsupported operating system: " + os);
        }

        arch = arch.replace("amd64", "x86_64").replace("aarch64", "arm64");

        String libPath = "/native/" + platform + "-" + arch + "/libdemangle";
        if (platform.contains("macos")) {
            libPath += ".dylib";
        } else {
            libPath += ".so";
        }

        try {
            System.load(extractResource(libPath).getAbsolutePath());
        } catch (IOException e) {
            throw new RuntimeException("Failed to load native library", e);
        }
    }

    private static File extractResource(String resourcePath) throws IOException {
        try (InputStream in = Demangler.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                throw new IOException("Resource not found: " + resourcePath);
            }
            File temp = File.createTempFile("libdemangle", getFileExtension(resourcePath));
            try (FileOutputStream out = new FileOutputStream(temp)) {
                byte[] buffer = new byte[4096];
                int bytesRead;
                while ((bytesRead = in.read(buffer)) != -1) {
                    out.write(buffer, 0, bytesRead);
                }
            }
            temp.deleteOnExit();
            return temp;
        }
    }

    private static String getFileExtension(String path) {
        return path.substring(path.lastIndexOf('.'));
    }

    public static String demangle(String in) {
        if (!SUCCESS) {
            return in;
        }
        if (in == null) {
            return in;
        }
        int center = in.indexOf("><");
        if (center == -1) {
            // just symbol
            String result = nativeDemangle(in);
            return result != null ? result : in;
        } else {
            // <library><symbol>
            String library = in.substring(0, center);
            String method = in.substring(center + 2, in.length() - 1);
            String result = nativeDemangle(method);
            if (result == null) {
                return in;
            }
            return library + "><" + result + ">";
        }
    }

    private static native String nativeDemangle(String in);
}
