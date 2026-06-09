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

import static com.bytedance.harmony.trace.cli.OHTraceConvert.clipJsMethodDesc;

import com.bytedance.harmony.trace.cli.os.OS;

import org.apache.commons.io.IOUtils;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;

public class Symbol implements SamplingMappingDecoder.OnDecodeListener,
                               StackListParser.OnParserListener {

    private final String mappingPath;

    private final String nameCachePath;

    private final String nativePath;

    private final boolean allSymbol;

    static private String sAddr2line;

    static {
        OS os = OS.get();
        String path = System.getenv(os.pathKeyName());
        String[] paths = path.split(os.pathSeparator());

        for (String p : paths) {
            File file = new File(p, os.addr2lineExecName());

            if (file.exists()) {
                sAddr2line = file.getAbsolutePath();
                Log.d("Got addr2line path: " + sAddr2line);
                break;
            }
        }

        if (sAddr2line == null) {
            throw new TraceError("addr2line not found in PATH",
                    "check you have export PATH with `/Applications/DevEco-Studio.app/"+
                            "Contents/sdk/default/openharmony/native/llvm/bin`.");
        }
    }


    public Symbol(String mappingPath, String nameCachePath, String nativePath, boolean allSymbol) {
        this.mappingPath = mappingPath;
        this.nameCachePath = nameCachePath;
        this.nativePath = nativePath;
        this.allSymbol = allSymbol;
    }

    Map<String, Map<Long, List<String>>> parseNativeSymbol(Map<Long, MethodSymbol> symbolMapping) {
        Map<String, File> soFileMap = new HashMap<>();

        Path startPath = Paths.get(nativePath);

        try (Stream<Path> pathStream = Files.walk(startPath)) {
            pathStream.forEach(path -> {
                if (Files.isRegularFile(path)) {
                    File f = path.toFile();
                    Log.i("Find " + f.getName() + " in native so dir");
                    soFileMap.put(f.getName(), f);
                }
            });
        } catch (IOException e) {
            System.err.println("Error walking directory: " + e.getMessage());
        }

        Map<String, List<Long>> libraryAndOffsets = groupLibrariesOffset(symbolMapping);

        return retrace(libraryAndOffsets, soFileMap);
    }

    @Override
    public void onDecodeFinish(SamplingMappingDecoder decoder) throws IOException {
        // preprocessing
        for (MethodSymbol symbol : decoder.symbolMapping.values()) {
            if (symbol.isNativeSymbol()) {
                String soName = stripSoPath(symbol.nativeReTracedResult.library);
                int middle = symbol.symbol.indexOf("><");
                String methodName = symbol.symbol.substring(middle + 2, symbol.symbol.length() - 1);
                symbol.symbol = "<" + soName+ "><" + methodName + ">";
                symbol.nativeReTracedResult = new MethodSymbol.NativeReTracedResult(soName, methodName);
            }
        }

        Map<String, Map<Long, List<String>>> libraryAndNames = null;

        if (!nativePath.isEmpty()) {
            libraryAndNames = parseNativeSymbol(decoder.symbolMapping);
        }

        Map<String, String> sourceMapping = null;

        if (!mappingPath.isEmpty()) {
            sourceMapping = new HashMap<>();

            InputStream is = new FileInputStream(mappingPath);
            String jsonTxt = IOUtils.toString(is, StandardCharsets.UTF_8);
            JSONObject jsonObj = new JSONObject(jsonTxt);

            for (String jsPath : jsonObj.keySet()) {
                JSONObject value = jsonObj.getJSONObject(jsPath);
                JSONArray sourceArr = value.getJSONArray("sources");

                assert (sourceArr.length() == 1);
                String oriJsPath = (String) sourceArr.get(0);
                sourceMapping.put(jsPath, oriJsPath);
            }
        }

        Map<String, Map<String, String>> nameCacheMap = null;

        if (!nameCachePath.isEmpty()) {
            nameCacheMap = new HashMap<>();

            InputStream is = new FileInputStream(nameCachePath);
            String jsonTxt = IOUtils.toString(is, StandardCharsets.UTF_8);
            JSONObject jsonObj = new JSONObject(jsonTxt);

            for (String oriJsPath : jsonObj.keySet()) {
                Map<String, String> methodMap = new HashMap<>();
                JSONObject value = jsonObj.optJSONObject(oriJsPath);

                if (value == null) {
                    continue;
                }

                JSONObject methodMapObj = value.optJSONObject("MemberMethodCache");

                if (methodMapObj == null) {
                    continue;
                }

                for (String oriMethod : methodMapObj.keySet()) {
                    methodMap.put(methodMapObj.getString(oriMethod), oriMethod);
                }

                JSONObject IdentifierCacheObj = value.getJSONObject("IdentifierCache");

                for (String oriIdentifier : IdentifierCacheObj.keySet()) {
                    methodMap.put(IdentifierCacheObj.getString(oriIdentifier), oriIdentifier);
                }

                nameCacheMap.put(oriJsPath, methodMap);
            }
        }

        for (MethodSymbol symbol : decoder.symbolMapping.values()) {
            if (symbol.isNativeSymbol()) {
                if (nativePath.isEmpty()) {
                    continue;
                }

                decodeNativeSymbol(symbol, libraryAndNames);
            } else {
                decodeMappingSymbol(symbol, sourceMapping, nameCacheMap);
            }
        }
    }

    @Override
    public void onParseFinish(List<StackList> stackLists) {
        for (StackList stackList : stackLists) {
            List<StackList.StackItem> expand = new ArrayList<>();
            for (StackList.StackItem item : stackList.stackTrace) {
                MethodSymbol.NativeReTracedResult nrr = null;

                if (item.method.isNativeSymbol()) {
                    nrr = item.method.nativeReTracedResult;

                    if (!(allSymbol ||
                        item.method.nativeReTracedResult.symbol.startsWith("main") ||
                        item.method.raw.startsWith("</data/storage"))) {
                        continue;
                    }
                }

                if (nrr != null && !nrr.retracedSymbol.isEmpty()) {
                    for (String symbol : nrr.retracedSymbol) {
                        MethodSymbol ms = new MethodSymbol(item.method.ptr, item.method.globalID, symbol);
                        ms.raw = item.method.raw;
                        ms.nativeReTracedResult.address = nrr.address;
                        expand.add(new StackList.StackItem(ms));
                    }
                } else {
                    expand.add(item);
                }
            }
            stackList.stackTrace = expand;
        }
    }

    private void decodeNativeSymbol(MethodSymbol symbol,
                                    Map<String, Map<Long, List<String>>> libraryAndNames) {
        MethodSymbol.NativeReTracedResult nrr = symbol.nativeReTracedResult;
        List<String> retraced = libraryAndNames.getOrDefault(nrr.library,
                                Collections.emptyMap()).get(nrr.address);
        if (retraced != null) {
            assert nrr.demangledSymbol.isEmpty();
            nrr.retracedSymbol.addAll(retraced);
            Collections.reverse(nrr.retracedSymbol);
        } else { // no need retrace or failed.
            nrr.retracedSymbol.add(symbol.symbol);
        }
    }

    private void decodeMappingSymbol(MethodSymbol symbol, Map<String, String> sourceMapping,
                                     Map<String, Map<String, String>> nameCacheMap) {

        int beginIdx = symbol.symbol.indexOf('<');
        int endIdx = symbol.symbol.indexOf(':');
        String jsMethod = symbol.symbol.substring(0, beginIdx);
        String jsPath = symbol.symbol.substring(beginIdx+1, endIdx);

        if (sourceMapping != null) {
            String oriJsPath = sourceMapping.get(jsPath);

            if (oriJsPath != null) {
                jsPath = oriJsPath;

                if (nameCacheMap != null) {
                    var methodCacheMap = nameCacheMap.get(jsPath);

                    if (methodCacheMap != null) {
                        String oriJsMethod = methodCacheMap.get(jsMethod);

                        if (oriJsMethod != null) {
                            jsMethod = oriJsMethod;
                            jsMethod = jsMethod.split(":")[0];
                        }
                    }
                }
            }
        }

        String fullName = jsMethod + "<" + jsPath + ":0>";
        symbol.symbol = clipJsMethodDesc(fullName);
    }

    private Map<String, Map<Long, List<String>>> retrace(Map<String, List<Long>> libraryAndOffsets,
                                                         Map<String, File> soFileMap) {
        Map<String, Map<Long, List<String>>> result = new HashMap<>();

        for (Map.Entry<String, List<Long>> entry : libraryAndOffsets.entrySet()) {
            String libraryName = entry.getKey();
            File libraryFile = soFileMap.get(libraryName);

            if (libraryFile == null || !libraryFile.exists() || libraryFile.length() == 0) {
                continue;
            }

            Log.i("Parse native symbol in: " + libraryName);

            try {
                List<String> cmd = new ArrayList<>();
                cmd.add(sAddr2line);
                cmd.add("-e");
                cmd.add(libraryFile.getAbsolutePath());
                cmd.add("-i");
                cmd.add("-p");
                cmd.add("-f");
                cmd.add("-a");

                for (Long addr : entry.getValue()) {
                    cmd.add(Long.toHexString(addr));
                }

                ProcessBuilder pb = new ProcessBuilder(cmd);// ignore_security_alert_wait_for_fix [ByDesign12.1]UsingProcessBuilder
                pb.redirectErrorStream(true);
                Process process = pb.start();
                BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
                String line;
                List<String> output = new ArrayList<>();

                while ((line = reader.readLine()) != null) {
                    output.add(line);
                }

                Map<Long, List<String>> parse = parse(output, libraryName);
                result.put(libraryName, parse);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return result;
    }

    public static Map<Long, List<String>> parse(List<String> lines, String libraryName) {
        Map<Long, List<String>> result = new HashMap<>();
        String currentAddr = null;
        List<String> currentNames = null;

        for (String line : lines) {
            if (line.startsWith("0x") && line.indexOf(": ") > 0) {
                if (currentAddr != null) {
                    result.put(Long.parseUnsignedLong(currentAddr.substring(2), 16),
                            currentNames);
                }

                int colonPos = line.indexOf(':');
                currentAddr = line.substring(0, colonPos);
                int atPos = line.lastIndexOf(" at ");
                String name = (atPos != -1) ?
                        line.substring(colonPos + 2, atPos) :
                        line.substring(colonPos + 2).trim();
                name = Demangler.demangle(name);
                name = "<" + libraryName + "><" + name + ">";
                currentNames = new ArrayList<>();
                currentNames.add(name);
                continue;
            }

            if (line.contains("(inlined by)")) {
                if (currentAddr == null) {
                    throw new RuntimeException("unexpected state");
                }

                int start = line.indexOf("(inlined by)") + 12;
                while (start < line.length() && line.charAt(start) == ' ') start++;

                int atPos = line.lastIndexOf(" at ");
                String name = (atPos != -1) ?
                        line.substring(start, atPos) :
                        line.substring(start).trim();
                name = "<" + libraryName + "><" + name + ">";
                currentNames.add(name);
                continue;
            }

            if (currentAddr != null && line.contains(" at :?")) {
                System.out.println("?");
            }
        }

        // 添加最后一个地址
        if (currentAddr != null) {
            result.put(Long.parseUnsignedLong(currentAddr.substring(2), 16),
                    currentNames);
        }
        return result;
    }

    private Map<String, List<Long>> groupLibrariesOffset(Map<Long, MethodSymbol> symbolMapping) {
        var libraryAndOffsets = new HashMap<String, List<Long>>();

        for (MethodSymbol symbol : symbolMapping.values()) {
            if (symbol.isNativeSymbol()) {
                libraryAndOffsets.computeIfAbsent(symbol.nativeReTracedResult.library,
                        k -> new ArrayList<>()).add(symbol.nativeReTracedResult.address);
            }
        }
        return libraryAndOffsets;
    }

    private static String stripSoPath(String fullSoPath) {
        int splitPos = fullSoPath.lastIndexOf('/');
        if (splitPos < 0) {
            return fullSoPath;
        }
        return fullSoPath.substring(splitPos + 1);
    }
}
