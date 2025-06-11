/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.bytedance.rheatrace.trace;

import com.bytedance.rheatrace.core.Arguments;
import com.bytedance.rheatrace.core.TraceError;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.LineIterator;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

public class ProguardMappingDecoder {

    private final File mappingFile;

    private final Map<String, MappingClass> classMap;

    private final LineInfo currentLine = new LineInfo();

    ProguardMappingDecoder() {
        mappingFile = new File(Arguments.get().mappingPath);
        classMap = new HashMap<>();
    }

    public void decode() throws IOException {
        if (!mappingFile.exists()) {
            throw new TraceError("mapping file (specified by -m option) does not exist: " + mappingFile.getAbsolutePath(), null);
        }
        if (!mappingFile.isFile()) {
            throw new TraceError("mapping file (specified by -m option) is not a file: " + mappingFile.getAbsolutePath(), null);
        }
        try (LineIterator it = FileUtils.lineIterator(mappingFile)) {
            moveToNextLine(it);
            while (currentLine.type != LineType.LINE_EOF) {
                if (currentLine.type == LineType.CLASS_LINE) {
                    // Match line "original_classname -> obfuscated_classname:".
                    String content = currentLine.content;
                    int arrowPos = content.indexOf(" -> ");
                    int arrowPosEnd = arrowPos + 4;
                    int colonPos = content.indexOf(':', arrowPosEnd);
                    if (colonPos != -1) {
                        String originalClassName = content.substring(0, arrowPos);
                        String obfuscatedClassName = content.substring(arrowPosEnd, colonPos);
                        MappingClass mappingClass = classMap.computeIfAbsent(obfuscatedClassName, (k) -> new MappingClass(originalClassName, false));
                        classMap.put(obfuscatedClassName, mappingClass);
                        moveToNextLine(it);
                        if (currentLine.type == LineType.SYNTHESIZED_COMMENT) {
                            mappingClass.synthesized = true;
                            moveToNextLine(it);
                        }
                        while (currentLine.type == LineType.METHOD_LINE) {
                            parseMethod(mappingClass, it);
                        }
                        continue;
                    }
                }

                // Skip unparsed line.
                moveToNextLine(it);
            }
        }
    }

    public String retrace(String methodSymbol) {
        int leftBracePos = methodSymbol.lastIndexOf('(');
        if (leftBracePos != -1) {
            String obfuscatedMethodName = methodSymbol.substring(0, leftBracePos);
            int spacePos = obfuscatedMethodName.indexOf(' ');
            if (spacePos != -1) {
                obfuscatedMethodName = obfuscatedMethodName.substring(spacePos + 1);
            }
            int classNameSep = obfuscatedMethodName.lastIndexOf('.');
            if (classNameSep != -1) {
                String obfuscatedClassName = obfuscatedMethodName.substring(0, classNameSep);
                String obfuscatedMethodNameWithoutClass = obfuscatedMethodName.substring(classNameSep + 1);
                MappingClass mappingClass = classMap.get(obfuscatedClassName);
                if (mappingClass != null) {
                    List<MappingMethod> candidates = mappingClass.methodMap.get(obfuscatedMethodNameWithoutClass);
                    if (candidates != null) {
                        if (candidates.size() == 1) {
                            return methodSignature(mappingClass, candidates.get(0));
                        } else {
                            int rightBracePos = methodSymbol.indexOf(')', leftBracePos);
                            if (rightBracePos != -1) {
                                String[] retraceParams;
                                String obfuscatedParamsStr = methodSymbol.substring(leftBracePos + 1, rightBracePos);
                                if (obfuscatedParamsStr.isEmpty()) {
                                    retraceParams = null;
                                } else {
                                    String[] obfuscatedParams = obfuscatedParamsStr.replace(" ", "").split(",");
                                    retraceParams = retraceParams(obfuscatedParams);
                                }
                                for (MappingMethod candidate : candidates) {
                                    if (candidate.paramsMatch(retraceParams)) {
                                        return methodSignature(mappingClass, candidate);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return methodSymbol;
    }

    private String[] retraceParams(String[] obfuscatedParams) {
        if (obfuscatedParams == null || obfuscatedParams.length == 0) {
            return null;
        }
        String[] retraceParams = new String[obfuscatedParams.length];
        for (int i = 0; i < obfuscatedParams.length; i++) {
            String obfuscatedParam = obfuscatedParams[i];
            MappingClass mappingClass = classMap.get(obfuscatedParam);
            if (mappingClass != null) {
                retraceParams[i] = mappingClass.originalName;
            } else {
                retraceParams[i] = obfuscatedParam;
            }
        }
        return retraceParams;
    }

    private String methodSignature(MappingClass mappingClass, MappingMethod method) {
        StringBuilder sb = new StringBuilder();
        if (!method.containsClassName) {
            sb.append(mappingClass.originalName);
            sb.append('.');
        }
        sb.append(method.originalName);
        sb.append('(');
        if (method.originalParams != null) {
            for (int i = 0; i < method.originalParams.length - 1; i++) {
                sb.append(method.originalParams[i]);
                sb.append(", ");
            }
            sb.append(method.originalParams[method.originalParams.length - 1]);
        }
        sb.append(')');
        return sb.toString();
    }


    private void parseMethod(MappingClass mappingClass, LineIterator iterator) {
        String content = currentLine.content;
        int arrowPos = content.indexOf(" -> ");
        int arrowPosEnd = arrowPos + 4;
        int leftBracePos = content.lastIndexOf('(');
        if (leftBracePos != -1) {
            int rightBracePos = content.lastIndexOf(')');
            String[] params = null;
            if (rightBracePos != -1 && rightBracePos > leftBracePos) {
                String paramsStr = content.substring(leftBracePos + 1, rightBracePos);
                if (!paramsStr.isEmpty()) {
                    params = paramsStr.replace(" ", "").split(",");
                }
            }
            int spacePos = content.substring(0, leftBracePos).lastIndexOf(' ', leftBracePos);
            if (spacePos!= -1) {
                String originalMethodName = content.substring(spacePos + 1, leftBracePos);
                int classNameSep = originalMethodName.lastIndexOf('.');
                boolean containsClassName = classNameSep != -1;
                if (containsClassName) {
                    String originalClassName = originalMethodName.substring(0, classNameSep);
                    if (mappingClass.originalName.equals(originalClassName)) {
                        originalMethodName = originalMethodName.substring(classNameSep + 1);
                        containsClassName = false;
                    }
                }
                String obfuscatedMethodName = content.substring(arrowPosEnd);
                boolean synthesized = false;
                moveToNextLine(iterator);
                if (currentLine.type == LineType.SYNTHESIZED_COMMENT) {
                    synthesized = true;
                    moveToNextLine(iterator);
                }
                MappingMethod mappingMethod = new MappingMethod(originalMethodName, params, synthesized, containsClassName);
                mappingClass.methodMap.computeIfAbsent(obfuscatedMethodName, (k) -> new LinkedList<>()).add(mappingMethod);
                return;
            }
        }
        // Skip unparsed line.
        moveToNextLine(iterator);
    }

    private void moveToNextLine(LineIterator iterator) {
        while (iterator.hasNext()) {
            String line = iterator.next();
            if (line.isEmpty()) {
                continue;
            }
            int nonSpacePos = firstNonSpaceIndex(line);
            if (nonSpacePos != -1 && line.charAt(nonSpacePos) == '#') {
                // Skip all comments unless it's synthesized comment.
                if (line.contains("com.android.tools.r8.synthesized")) {
                    currentLine.type = LineType.SYNTHESIZED_COMMENT;
                    currentLine.content = line;
                    return;
                }
                continue;
            }
            if (!line.contains(" -> ")) {
                // Skip unknown lines.
                continue;
            }
            currentLine.content = line;
            if (line.charAt(0) == ' ') {
                currentLine.type = LineType.METHOD_LINE;
            } else {
                currentLine.type = LineType.CLASS_LINE;
            }
            return;
        }
        currentLine.type = LineType.LINE_EOF;
    }

    private int firstNonSpaceIndex(String s) {
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) != ' ') {
                return i;
            }
        }
        return -1;
    }

    enum LineType {
        SYNTHESIZED_COMMENT,
        CLASS_LINE,
        METHOD_LINE,
        LINE_EOF,
    }

    static class LineInfo {
        LineType type;
        String content;
    }

    static class MappingMethod {
        final String originalName;
        final String[] originalParams;
        final boolean synthesized;
        final boolean containsClassName;

        MappingMethod(String originalName, String[] originalParams, boolean synthesized, boolean containsClassName) {
            this.originalName = originalName;
            this.originalParams = originalParams;
            this.synthesized = synthesized;
            this.containsClassName = containsClassName;
        }

        boolean paramsMatch(String[] params) {
            int thisParamCount = originalParams == null ? 0 : originalParams.length;
            int compareParamCount = params == null? 0 : params.length;
            if (thisParamCount == 0 && compareParamCount == 0) {
                return true;
            }
            if (thisParamCount != compareParamCount) {
                return false;
            }
            for (int i = 0; i < thisParamCount; i++) {
                if (!originalParams[i].equals(params[i])) {
                    return false;
                }
            }
            return true;

        }
    }

    static class MappingClass {
        final String originalName;
        boolean synthesized;

        final Map<String, List<MappingMethod>> methodMap;

        MappingClass(String originalName, boolean synthesized) {
            this.originalName = originalName;
            this.synthesized = synthesized;
            methodMap = new HashMap<>();
        }
    }
}