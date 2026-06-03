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

import { TraceAppMeta } from "./TraceAppMeta";
import { SceneConfig } from "./SceneConfig";
import { TraceStorage } from "./TraceStorage";
import { Request } from "./Request"
import { TraceUploader } from "./TraceUploader";
import systemDateTime from "@ohos.systemDateTime";
import { hilog } from "@kit.PerformanceAnalysisKit";
import { EventRegistry } from "./EventRegistry";
import { AppEventCallbackImpl, FirstFrameParam, FrameDropParam } from "./AppEventCallback";
import { TraceAbility } from "./TraceAbility";
import { TraceEventFile } from "./TraceEventFile";
import { collections, taskpool } from "@kit.ArkTS";
import { asyncDump } from "./asyncs/Functions";
import { FunctionSwitches } from "./FunctionSwitches";
import { DefaultEventRegistry } from "./DefaultEventRegistry";


@Sendable
export class TraceSceneManager {

  private static instance: TraceSceneManager | undefined;

  private storage: TraceStorage;
  private uploader: TraceUploader;
  private currentDumpCount: number = 0;
  private functionSwitches: FunctionSwitches;
  private dumpSampling: collections.Map<number, number> | undefined = undefined;

  private constructor(traceStorage: TraceStorage, traceUploader: TraceUploader) {
    this.storage = traceStorage;
    this.uploader = traceUploader;
    this.functionSwitches = new FunctionSwitches();
  }

  static getInstance(): TraceSceneManager | undefined {
    return TraceSceneManager.instance;
  }

  static init(appFileDir: string, sceneConfig: SceneConfig, appMeta: TraceAppMeta,
              request: Request): TraceSceneManager | undefined {
    if (!sceneConfig.enabled) {
      return undefined;
    }

    let traceStorage = TraceStorage.init(appFileDir);
    let traceUploader = TraceUploader.init(appMeta, request);
    return new TraceSceneManager(traceStorage, traceUploader);
  }

  getAppFreezeBaseDir(): string {
    return this.storage.getBaseDir();
  }

  getFunctionSwitches(): FunctionSwitches {
    return this.functionSwitches;
  }

  getSampleRate(sceneId: number): number {
    if (this.dumpSampling) {
      try {
        if (this.dumpSampling.has(sceneId)) {
          return this.dumpSampling.get(sceneId) as number;
        }
      } catch (error) {
      }
      return 0.0;
    }
    return 1.0;
  }

  refundDump() {
    if (0 < this.currentDumpCount) {
      this.currentDumpCount--;
    }
  }

  async registerScenes(sceneConfig: SceneConfig, appMeta: TraceAppMeta, eventRegistry: EventRegistry | undefined) {
    this.dumpSampling = sceneConfig.getDumpSampling();

    if (!eventRegistry) {
      eventRegistry = new DefaultEventRegistry();
    }

    let callback = new AppEventCallbackImpl(this);
    let appFreezeRegistered = false;

    // 1. Custom AppFreeze Monitor has higher priority, independent of isAppFreezeEnabled()
    if (sceneConfig.isCustomAppFreezeMonitor()) {
    }

    // 2. If custom is disabled or failed to register, fallback to standard listener if it is enabled
    if (!appFreezeRegistered && sceneConfig.isAppFreezeEnabled()) {
      appFreezeRegistered = eventRegistry.registerAppFreezeListener(callback);
    }

    if (appFreezeRegistered) {
      this.functionSwitches.markAnrEnabled();
    }

    if (sceneConfig.enableFrameDrop) {
      if (eventRegistry.registerFrameDropListener(callback, sceneConfig.getFrameDropThresMs())) {
        this.functionSwitches.markFrameDropEnabled();
      }
    }

    if (sceneConfig.enableFirstFrame) {
      if (eventRegistry.registerFirstFrameListener(callback, sceneConfig.getFirstFrameDelayS())) {
        // this.functionSwitches.markFrameDropEnabled();
      }
    }
  }

  getLegacyEventTraces(): TraceEventFile[] {
    let eventFiles: TraceEventFile[] = this.storage.listTraceFiles();
    return eventFiles;
  }

  requestDump(): boolean {
    if (2 < this.currentDumpCount) {
      return false;
    }

    this.currentDumpCount++;
    return true;
  }

  async dumpAndUpload(id: number, traceEventFile: TraceEventFile | undefined, currTimeMs: number = 0,
                      extraObj: Record<string, Object> | undefined = undefined,
                      tids: number[]=[], beginTimeMs: number=0, endTimeMs: number=0): Promise<boolean> {
    let sampleRate = this.getSampleRate(id);
    if (sampleRate < Math.random()) {
      return false;
    }

    if (!this.requestDump()) {
      return false;
    }

    let extraStr = this.makeExtra(id, currTimeMs, extraObj)

    let res = true;

    try {
      let realTraceFile = await taskpool.execute(asyncDump, id, traceEventFile, extraStr, this, tids, beginTimeMs,
                                                endTimeMs) as TraceEventFile | undefined
      if (!realTraceFile) {
        res = false;
      } else {
        res = await this.upload(id, realTraceFile);
      }
    } catch (e) {
      res = false;
    } finally {
      this.refundDump();
    }

    return res;
  }

  onAppFreeze(): string {
    let currTimeMs = systemDateTime.getTime()
    let traceEventFile = this.storage.makeAppFreezeDir(currTimeMs);
    let sceneId = 200
    this.dumpAndUpload(sceneId, traceEventFile, currTimeMs);
    return `${this.uploader.getAppMeta().deviceId}-${currTimeMs}`;
  }

  onFrameDrop(params: FrameDropParam): string {
    let currTimeMs = systemDateTime.getTime()
    let customExtra: Record<string, Object> = {
      "scene": params.scene
    };

    let sceneId = 120
    this.dumpAndUpload(sceneId, undefined, currTimeMs, customExtra,
                      [], params.beginTimeMs, params.endTimeMs);
    return `${this.uploader.getAppMeta().deviceId}-${currTimeMs}`;
  }

  onFirstFrame(params: FirstFrameParam): string {
    let currTimeMs = systemDateTime.getTime()
    let phaseInfo: Record<string, FirstFrameParam> = {
      "phase": params
    }
    let customExtra: Record<string, Object> = {
      "extra": phaseInfo
    };

    let sceneId = 110
    this.dumpAndUpload(sceneId, undefined, currTimeMs, customExtra,
      [], 0, currTimeMs);
    return `${this.uploader.getAppMeta().deviceId}-${currTimeMs}`;
  }

  async upload(id: number, traceEventFile: TraceEventFile): Promise<boolean> {
    let res = true;
    let doCleanUp = true;

    try {
      let finalFile = await this.uploader.upload(id, traceEventFile)

      if (finalFile) {
        hilog.info(0x0000, 'btrace:scene', `upload trace finish`);
        res = true;
      }
    } catch (e) {
      if (id != 200) {
        doCleanUp = false
      }
      hilog.error(0x0000, 'btrace:scene', `upload trace failed ${e}`);
      res = false;
    }

    if (doCleanUp) {
      await this.storage.asyncCleanupTraceFile(traceEventFile);
    }

    return res
  }

  static cleanupLegacyEventTraces(appFileDir: string): number {
    let traceStorage = TraceStorage.makePhantom(appFileDir);
    let traceFiles = traceStorage.listTraceFiles()
    for (let traceFile of traceFiles) {
      traceStorage.asyncCleanupTraceFile(traceFile);
    }
    return traceFiles.length;
  }

  makeTraceFile(id: number, time: number): TraceEventFile {
    return this.storage.makeTraceFile(id, time);
  }

  makeExtra(id: number, time: number, customExtra?: Record<string, Object>): string | undefined{
    return this.uploader.makeExtra(id, time, customExtra);
  }
}
