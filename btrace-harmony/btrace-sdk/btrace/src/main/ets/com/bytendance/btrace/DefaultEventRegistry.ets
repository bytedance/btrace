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

import { EventRegistry } from "./EventRegistry";
import { errorManager } from "@kit.AbilityKit";
import { AppEventCallback, AppEventType, FrameDropParam } from "./AppEventCallback";
import { displaySync } from '@kit.ArkGraphics2D';
import { hilog } from "@kit.PerformanceAnalysisKit";
import systemDateTime from "@ohos.systemDateTime";

export class DefaultEventRegistry implements EventRegistry {

  private lastFrameTimeMs: number = 0;
  private currentScene: string = 'unknown';
  private frameMonitor: displaySync.DisplaySync | undefined = undefined;

  registerAppFreezeListener(callback: AppEventCallback): boolean {
    errorManager.on('freeze', () => {
      callback.onAppEvent(AppEventType.APP_FREEZE, new Object());
    });
    return true;
  }

  registerFrameDropListener(callback: AppEventCallback, thresholdMs: number): boolean {
    try {
      if (this.frameMonitor) {
        this.frameMonitor.stop()
      }

      this.frameMonitor = displaySync.create();
      this.frameMonitor.on('frame', (intervalInfo: displaySync.IntervalInfo) => {
        let currentTimeMs = Math.floor(intervalInfo.timestamp / 1000 / 1000);

        if (this.lastFrameTimeMs !== 0) {
          let frameDurMs = currentTimeMs - this.lastFrameTimeMs;

          if (0 < thresholdMs && thresholdMs < frameDurMs) {
            let currUtcTimeMs = systemDateTime.getTime();
            let hitchParam: FrameDropParam = {
              beginTimeMs: currUtcTimeMs - frameDurMs,
              endTimeMs: currUtcTimeMs,
              scene: this.currentScene
            }
            callback.onAppEvent(AppEventType.HITCH, hitchParam);
          }
        }
        this.lastFrameTimeMs = currentTimeMs;
      });

      this.frameMonitor.start();
    } catch (e) {
      hilog.error(0x0000, 'btrace:registry', 'register default hitch listener failed: %{public}s', e);
      return false;
    }
    return true;
  }

  registerFirstFrameListener(callback: AppEventCallback, delayS: number): boolean {
    setTimeout(() => {
      // callback.onAppEvent(AppEventType.FIRST_FRAME, new Object());
    }, delayS*1000)
    return true;
  }
}