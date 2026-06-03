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

import { collections } from "@kit.ArkTS";

@Sendable
export class SceneConfig {
  enabled: boolean = false;
  enableAppFreeze: boolean = false;
  customAppFreezeMonitor: boolean = false;
  enableFrameDrop: boolean = false;
  frameDropThresMs: number = 0;
  enableFirstFrame: boolean = false;
  firstFrameDelayS: number = 0;
  dumpSampling: collections.Map<number, number> | undefined = undefined;

  setEnable(enabled: boolean) {
    this.enabled = enabled;
  }

  isEnabled(): boolean {
    return this.enabled;
  }

  setAppFreezeEnable(enabled: boolean) {
    this.enableAppFreeze = enabled;
  }

  isAppFreezeEnabled(): boolean {
    return this.enableAppFreeze;
  }

  setCustomAppFreezeMonitor(custom: boolean) {
    this.customAppFreezeMonitor = custom;
  }

  isCustomAppFreezeMonitor(): boolean {
    return this.customAppFreezeMonitor;
  }

  setFrameDropEnable(enabled: boolean) {
    this.enableFrameDrop = enabled;
  }

  isFrameDropEnabled(): boolean {
    return this.enableFrameDrop;
  }

  setFrameDropThresMs(thresMs:number) {
    this.frameDropThresMs = thresMs;
  }

  getFrameDropThresMs(): number {
    return this.frameDropThresMs;
  }

  setFirstFrameEnable(enabled: boolean) {
    this.enableFirstFrame = enabled;
  }

  isFirstFrameEnabled(): boolean {
    return this.enableFirstFrame;
  }

  setFirstFrameDelayS(thresMs:number) {
    this.firstFrameDelayS = thresMs;
  }

  getFirstFrameDelayS(): number {
    return this.firstFrameDelayS;
  }

  setDumpSampling(dumpSampling: Record<string, number> | undefined) {
    if (!dumpSampling) { return; }

    this.dumpSampling = new collections.Map<number, number>()

    for (let key of Object.keys(dumpSampling)) {
      let value = dumpSampling[key];
      let sceneId = parseInt(key, 10)

      if (Number.isNaN(sceneId)) {
        return;
      }

      try {
        this.dumpSampling.set(sceneId, value)
      } catch (error) {
        // TODO: Implement error handling.
      }
    }
  }

  getDumpSampling(): collections.Map<number, number> | undefined {
    return this.dumpSampling;
  }
}