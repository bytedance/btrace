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

import static com.bytedance.harmony.trace.cli.CallNode.kMalloc;

import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

public class StackListParser implements StackListEncoder.TraceEncoderProvider {
    public interface OnParserListener {
        void onParseFinish(List<StackList> stackLists);
    }

    private static OnParserListener sOnParserListener;

    public static void setOnParserListener(OnParserListener onParserListener) {
        sOnParserListener = onParserListener;
    }

    public static final int OS_ANDROID = 0;
    public static final int OS_IOS = 2;
    public static final int OS_HARMONY = 4;

    public SamplingFile samplingFile;
    private int processId;
    private int sceneId;
    private long utcTimeDiff;
    private SamplingMappingDecoder mappingDecoder;
    private int os;
    private int version;
    private long currentTimeMs;
    private JSONObject extra;
    private List<StackList> stackLists;
    private final List<StackList> mainStackLists = new ArrayList<>();
    private final Set<Integer> threadIds = new HashSet<>();
    private boolean revised;
    private boolean hasNative;
    private final StackParseFactory stackParseFactory;
    private int dataSize;

    public StackListParser(StackParseFactory stackParseFactory) {
        this.stackParseFactory = stackParseFactory;
    }

    public StackListParser parse(SamplingFile sf) {
        samplingFile = sf;
        ByteBuffer buffer = ByteBuffer.wrap(sf.sampling).order(ByteOrder.LITTLE_ENDIAN);
        int magic = buffer.getInt();
        short type = buffer.getShort();
        os = buffer.getShort();
        version = buffer.getInt();
        long time = buffer.getLong();
        int count = buffer.getInt();
        int extraLength = buffer.getInt();
        if (extraLength > 0) {
            byte[] b = new byte[extraLength];
            buffer.get(b);
            extra = new JSONObject(new String(b));
        } else {
            extra = new JSONObject();
        }
        mappingDecoder = new SamplingMappingDecoder(sf, Objects.toString(extra.opt("updateVersionCode"))).decode(os);
        currentTimeMs = extra.getLong("currentTimeMs");
        processId = extra.optInt("processId");
        sceneId = extra.getInt("sceneId");
        utcTimeDiff = extra.optLong("utcTimeDiff");
        JSONObject ex = this.extra.optJSONObject("extra");
        int spikeThreadId = ex == null ? 0 : ex.optInt("spikeThreadId");
        stackLists = parse(buffer, spikeThreadId);
        for (StackList stackList : stackLists) {
            if (stackList.tid == processId) {
                mainStackLists.add(stackList);
            }
            threadIds.add(stackList.tid);
        }
        return this;
    }

    private List<StackList> parse(ByteBuffer buffer, int targetThreadId) {
        List<StackList> result = new ArrayList<>();
        int index = 0;
        while (buffer.hasRemaining()) {
            index++;
            // 读完 buffer 一条记录
            SamplingStack m = new SamplingStack(buffer, os, version);
            if (targetThreadId > 0 && targetThreadId != m.tid) {
                continue;
            }

            int type = m.type;
            int tid = m.tid;
            hasNative = hasNative || type >= kMalloc;
            boolean isNativeStack = m.type >= kMalloc;
            StackList.StackItem[] stack = m.buildStackTrace(mappingDecoder);
            if (stack.length == 0) {
                continue;
            }
            if (stackParseFactory.get(sceneId).needKeepStack(this, isNativeStack, tid)) { // TODO 超长堆栈如何处理
                StackList item = new StackList(m.nanoTime, m.cpuTime, stack, tid, type).setMessageId(m.messageId);
                item.isNative = isNativeStack;
                result.add(item);
                if (m.nanoTimeEnd > m.nanoTime) { // 存在异常数据，nanoTimeEnd 小于 nanoTime 的情况，可能与时间戳同步有关，暂时忽略。
                    StackList end = item.dup(m.nanoTimeEnd, m.cpuTimeEnd);
                    result.add(end);
                }
            }
        }
        for (StackList stackList : result) {
            stackList.groupId = currentTimeMs;
            stackList.pid = processId;
        }
        result.sort(Comparator.comparingLong(o -> o.nanoTime));
        OnParserListener listener = sOnParserListener;
        if (listener != null) {
            listener.onParseFinish(result);
        }
        dataSize = index;
        return result;
    }

    private StackList.StackItem[] findPreJavaStack(List<StackList> result, int tid) {
        for (int i = result.size() - 1; i >= 0; i--) {
            StackList last = result.get(i);
            if (last != null && last.tid == tid && !last.isNative) {
                return last.getStackTrace().toArray(new StackList.StackItem[0]);
            }
        }
        return null;
    }

    public StackListParser revise() {
        if (hasNative) {
            stackLists = StacksReviserV2.revise(stackLists);
            revised = true;
        }
        return this;
    }

    public StackListParser reviseHarmony(boolean mainThreadOnlyKeepJsFrame) {
        if (os != OS_HARMONY) {
            return this;
        }
        List<StackList> newStackLists = new ArrayList<>();
        stackLists.forEach(s -> {
            if (s.pid == s.tid) {
                if (mainThreadOnlyKeepJsFrame) {
                    ArrayList<StackList.StackItem> stackItems = new ArrayList<>();
                    boolean hasMainThreadStartFrame = false;
                    for (StackList.StackItem item : s.stackTrace) {
                        if (!item.method.symbol.startsWith("<")) {
                            stackItems.add(item);
                        } else if (item.method.symbol.contains("<OHOS::AppExecFwk::MainThread::Start()>")
                                || item.method.symbol.contains("_ZN4OHOS10AppExecFwk10MainThread5StartEv")) {
                            hasMainThreadStartFrame = true;
                        }
                    }
                    if (hasMainThreadStartFrame) {
                        s.stackTrace = stackItems;
                        newStackLists.add(s);
                    }
                } else {
                    ArrayList<StackList.StackItem> stackItems = new ArrayList<>();
                    for (int i = s.stackTrace.size() - 1; i >= 0; --i) {
                        StackList.StackItem item = s.stackTrace.get(i);
                        if (item.method.symbol.contains("<OHOS::AppExecFwk::MainThread::Start()>")
                                || item.method.symbol.contains("_ZN4OHOS10AppExecFwk10MainThread5StartEv")) {
                            stackItems.add(item);
                            break;
                        }
                        stackItems.add(item);
                    }
                    if (!stackItems.isEmpty()) {
                        StackList.StackItem topItem = stackItems.get(stackItems.size() - 1);
                        if (topItem.method.symbol.contains("<OHOS::AppExecFwk::MainThread::Start()>")
                                || topItem.method.symbol.contains("_ZN4OHOS10AppExecFwk10MainThread5StartEv")) {
                            Collections.reverse(stackItems);
                            s.stackTrace = stackItems;
                            newStackLists.add(s);
                        }
                    }
                }
            } else {
                newStackLists.add(s);
            }
        });
        stackLists = newStackLists;
        return this;
    }

    public StackListParser trimEmpty() {
        List<StackList> result = new ArrayList<>();

        for (StackList item : stackLists) {
            if (item.stackTrace.isEmpty()) {
                continue;
            }
            result.add(item);
        }

        stackLists = result;
        return this;
    }

    public StackListParser retainTheLast(long dur) {
        long lastMainNano = 0;
        stackLists.sort(Comparator.comparingLong(o -> o.nanoTime));

        for (int i=stackLists.size()-1;0 <= i;--i) {
            StackList item = stackLists.get(i);

            if (item.tid == processId) {
                lastMainNano = item.nanoTime;
                break;
            }
        }

        long startTime = lastMainNano - dur;
        List<StackList> result = new ArrayList<>();

        for (StackList item : stackLists) {
            if (item.nanoTime < startTime) {
                continue;
            }
            result.add(item);
        }

        stackLists = result;
        return this;
    }

    public StackListParser trimFalseStart() {
        // 子线程早于主线程第一个栈的时间戳与主线程对齐
        stackLists.sort(Comparator.comparingLong(o -> o.nanoTime));
        long firstNano = 0;
        List<StackList> preMain = new ArrayList<>();
        for (StackList item : stackLists) {
            if (item.tid == processId) {
                firstNano = item.nanoTime;
                break;
            }
            preMain.add(item);
        }
        if (firstNano > 0) {
            for (StackList stackList : preMain) {
                if (stackList.end != null) { // 只调整 duration stack 的时间戳
                    stackList.nanoTime = firstNano;
                }
            }
        }
        return this;
    }

    public int getProcessId() {
        return processId;
    }

    public int getSceneId() {
        return sceneId;
    }

    public long getUtcTimeDiff() {
        return utcTimeDiff;
    }

    public SamplingMappingDecoder getMappingDecoder() {
        return mappingDecoder;
    }

    public int getOs() {
        return os;
    }

    public JSONObject getExtra() {
        return extra;
    }

    public List<StackList> getStackLists() {
        return stackLists;
    }

    @Override
    public Map<Integer, String> getThreadNames() {
        return getMappingDecoder().threadNames;
    }

    @Override
    public boolean isAndroid() {
        return os == OS_ANDROID;
    }

    public boolean isIOS() {
        return os == OS_IOS;
    }

    public Map<Integer, ThreadStackList> groupByThreadId() {
        stackLists.sort(Comparator.comparingLong(o -> o.nanoTime));
        Map<Integer, ThreadStackList> map = new HashMap<>();
        for (StackList item : stackLists) {
            ThreadStackList threadStackList = map.computeIfAbsent(item.tid, ThreadStackList::new);
            if (item.type < kMalloc || revised) {
                threadStackList.main.add(item);
            } else {
                threadStackList.cpp.add(item);
            }
        }
        ArrayList<ThreadStackList> sort = new ArrayList<>(map.values());
        sort.sort((a, b) -> Long.compare(b.cpuDuration(), a.cpuDuration()));
        for (int i = 0; i < sort.size(); i++) {
            ThreadStackList thread = sort.get(i);
            thread.cpuRank = i + 1;
        }
        return map;
    }

    public long getCurrentTimeMs() {
        return currentTimeMs;
    }

    public int getDataSize() {
        return dataSize;
    }

    public boolean hasNative() {
        return hasNative;
    }

    public List<StackList> getMainStackLists() {
        return Collections.unmodifiableList(mainStackLists);
    }

    public List<Integer> getThreadIds() {
        return new ArrayList<>(threadIds);
    }
}