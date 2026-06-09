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

import { BtraceInitParams } from '../BtraceInitParams';
import { TraceAbility } from '../TraceAbility';
import { TraceEventFile } from '../TraceEventFile';
import { TraceSceneManager } from '../TraceSceneManager';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { systemDateTime } from '@kit.BasicServicesKit';
import { JSON, process } from "@kit.ArkTS";
import { TraceConfig } from '../TraceConfig';
import { SceneConfig } from '../SceneConfig';
import { TraceAppMeta } from '../TraceAppMeta';
import { Request } from '../Request';
import fs from '@ohos.file.fs';

@Concurrent
export function asyncInit(appFileDir: string, traceConfig: TraceConfig, sceneConfig: SceneConfig,
                          appMeta: TraceAppMeta, request: Request): TraceSceneManager | undefined {
  if (!TraceAbility.init(traceConfig)) {
    return undefined;
  }

  let sceneManager = TraceSceneManager.init(appFileDir, sceneConfig, appMeta, request);
  if (!sceneManager) {
    TraceAbility.terminate();
    return undefined;
  }

  return sceneManager;
}

@Concurrent
export function getLegacyEventTraces(sceneManager: TraceSceneManager): TraceEventFile[] {
  return sceneManager.getLegacyEventTraces();
}

@Concurrent
export function cleanupLegacyEventTraces(appFileDir: string): number {
  return TraceSceneManager.cleanupLegacyEventTraces(appFileDir);
}

@Concurrent
export function asyncDump(id: number, traceEventFile: TraceEventFile | undefined, extra: string | undefined,
                              sceneManager: TraceSceneManager, tids: number[]=[], beginTimeMs: number=0,
                              endTimeMs: number=0): TraceEventFile | undefined {
  let time = 0

  if (!traceEventFile) {
    if (time == 0) {
      time = systemDateTime.getTime()
    }
    traceEventFile = sceneManager.makeTraceFile(id, time)
  }

  traceEventFile.sceneId = id

  if (!extra) {
    if (time == 0) {
      time = systemDateTime.getTime()
    }
    extra = sceneManager.makeExtra(id, time)
  }

  if (!traceEventFile || !extra) {
    return undefined;
  }

  hilog.info(0x0000, 'btrace:dump', `dump trace file name: ${id}_${traceEventFile.time}`);

  if (TraceAbility.dumpTrace(traceEventFile.dir, extra, tids, beginTimeMs, endTimeMs)) {
    return traceEventFile;
  } else {
    try {
      if (fs.accessSync(traceEventFile.dir)) {
        fs.rmdirSync(traceEventFile.dir);
      }
    } catch (error) {
      // TODO: Implement error handling.
    }
  }

  hilog.info(0x0000, 'btrace:dump', `dump failed, trace file name: ${id}_${traceEventFile.time}`);

  return undefined;
}

@Concurrent
export function asyncDumpTrace(targetDir: string): boolean {
  return TraceAbility.dumpTrace(targetDir, JSON.stringify({
    currentTimeMs: systemDateTime.getTime(),
    sceneId: 0,
    processId: process.pid
  }));
}