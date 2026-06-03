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

import { lang } from "@kit.ArkTS";
import { TraceSceneManager } from "./TraceSceneManager";

export enum AppEventType {
  APP_FREEZE = 1,
  HITCH = 2,
  FIRST_FRAME = 3
}

export class FrameDropParam {
  beginTimeMs: number = 0;
  endTimeMs: number = 0;
  scene: string = "";
}

export type FirstFrameParam = Record<string, [number, number, number, number]>

export interface AppEventCallback extends lang.ISendable {
  onAppEvent(type: AppEventType, params: object | undefined): string;
}

@Sendable
export class AppEventCallbackImpl implements AppEventCallback {

  private sceneManager: TraceSceneManager;

  constructor(sceneManger: TraceSceneManager) {
    this.sceneManager = sceneManger;
  }

  onAppEvent(type: AppEventType, param: object | undefined): string {
    if (type === AppEventType.APP_FREEZE) {
      return this.sceneManager.onAppFreeze();
    } else if (type === AppEventType.HITCH) {
      if (param) {
        let hitchParam = param as FrameDropParam;
        return this.sceneManager.onFrameDrop(hitchParam);
      }
      return '';
    } else if (type === AppEventType.FIRST_FRAME) {
      if (param) {
        let stageParam = param as FirstFrameParam;
        return this.sceneManager.onFirstFrame(stageParam);
      }
      return '';
    }
    return '';
  }
}
