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
import { systemDateTime } from '@kit.BasicServicesKit';
import { JSON, process } from "@kit.ArkTS";

export function syncInit(appFileDir: string, initParams: BtraceInitParams): TraceSceneManager | undefined {
  if (!TraceAbility.init(initParams.getTraceConfig()!)) {
    return undefined;
  }

  let sceneManager = TraceSceneManager.init(appFileDir, initParams.getSceneConfig(), initParams.getAppMeta(), initParams.getRequest());
  if (!sceneManager) {
    TraceAbility.terminate();
    return undefined;
  }

  return sceneManager;
}

export function syncGetLegacyEventTraces(sceneManager: TraceSceneManager): TraceEventFile[] {
  return sceneManager.getLegacyEventTraces();
}

export function syncCleanupLegacyEventTraces(appFileDir: string): number {
  return TraceSceneManager.cleanupLegacyEventTraces(appFileDir);
}

export function syncDumpAndUpload(id: number, traceEventFile: TraceEventFile, extra: string, sceneManager: TraceSceneManager) {
  if (TraceAbility.dumpTrace(traceEventFile.dir, extra)) {
    sceneManager.upload(id, traceEventFile);
  }
}

export function syncDumpTrace(targetDir: string): boolean {
  return TraceAbility.dumpTrace(targetDir, JSON.stringify({
    currentTimeMs: systemDateTime.getTime(),
    sceneId: 0,
    processId: process.pid
  }));
}