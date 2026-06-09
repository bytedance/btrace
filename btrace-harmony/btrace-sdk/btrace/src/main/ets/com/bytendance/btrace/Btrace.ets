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

import { TraceSceneManager } from './TraceSceneManager';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BtraceInitParams } from './BtraceInitParams';
import { DefaultEventRegistry } from './DefaultEventRegistry';
import { taskpool } from '@kit.ArkTS';
import { cleanupLegacyEventTraces, asyncInit, getLegacyEventTraces } from './asyncs/Functions';
import { TraceEventFile } from './TraceEventFile';
import { TraceAbility } from './TraceAbility';
import { postFunctionSwitchInfo } from './RequestHelper';
import { syncGetLegacyEventTraces, syncInit } from './asyncs/SyncFunctions';
import { deviceInfo } from '@kit.BasicServicesKit';

enum State {
  kUninitialized,
  kInitializing,
  kInitialized
}

const TAG = 'btrace:init';

export class Btrace {
  private static sceneManager: TraceSceneManager | undefined;
  private static state: State = State.kUninitialized;

  static async init(context: Context, initParams: BtraceInitParams): Promise<boolean> {
    if (Btrace.state != State.kUninitialized) {
      return Btrace.state === State.kInitialized;
    }

    let osVersionLimit = initParams.getOsVersionLimit()
    if (!isValidOsVersion(osVersionLimit)) {
      return false;
    }

    let appVersionCodeLimit = initParams.getAppVersionCodeLimit()
    if (appVersionCodeLimit && appVersionCodeLimit <= initParams.getAppMeta().updateVersionCode) {
      return false;
    }

    Btrace.state = State.kInitializing;
    Btrace.sceneManager = undefined;

    let sceneManager = await taskpool.execute(asyncInit, context.cacheDir, initParams.getTraceConfig(),
                       initParams.getSceneConfig(), initParams.getAppMeta(), initParams.getRequest()) as TraceSceneManager | undefined;
    if (!sceneManager) {
      Btrace.state = State.kUninitialized;
      return false;
    }

    let eventRegistry = initParams.getEventRegistry();
    if (!eventRegistry) {
      eventRegistry = new DefaultEventRegistry();
    }

    await sceneManager.registerScenes(initParams.getSceneConfig(), initParams.getAppMeta(), eventRegistry);

    // upload legacy traces
    let traceEventFiles = await taskpool.execute(getLegacyEventTraces, sceneManager) as TraceEventFile[];
    if (traceEventFiles.length > 0) {
      for (let eventFile of traceEventFiles) {
        sceneManager.upload(eventFile.sceneId, eventFile);
      }
    }

    // upload metrics info
    setTimeout((sceneManager: TraceSceneManager) => {
      postFunctionSwitchInfo(
        initParams.getRequest(),
        sceneManager.getFunctionSwitches().getTypes(),
        initParams.getAppMeta()
      ).then((success) => {
        hilog.info(0x0000, TAG, `function type report: ${success}`);
      }).catch((e: Error) => {
        hilog.info(0x0000, TAG, `function type report err: ${e}`);
      })
    }, 10 * 1000, sceneManager);

    Btrace.sceneManager = sceneManager;
    Btrace.state = State.kInitialized;
    hilog.info(0x0000, TAG, 'init success');

    return true;
  }

  static syncInit(context: Context, initParams: BtraceInitParams): boolean {
    if (Btrace.state != State.kUninitialized) {
      return Btrace.state === State.kInitialized;
    }

    let osVersionLimit = initParams.getOsVersionLimit()
    if (!isValidOsVersion(osVersionLimit)) {
      return false;
    }

    Btrace.state = State.kInitializing;
    Btrace.sceneManager = undefined;

    let sceneManager = syncInit(context.cacheDir, initParams);
    if (!sceneManager) {
      Btrace.state = State.kUninitialized;
      return false;
    }

    let eventRegistry = initParams.getEventRegistry();
    if (!eventRegistry) {
      eventRegistry = new DefaultEventRegistry();
    }

    sceneManager.registerScenes(initParams.getSceneConfig(), initParams.getAppMeta(), eventRegistry);

    let traceEventFiles = syncGetLegacyEventTraces(sceneManager);
    if (traceEventFiles.length > 0) {
      for (let eventFile of traceEventFiles) {
        sceneManager.upload(eventFile.sceneId, eventFile);
      }
    }

    setTimeout((sceneManager: TraceSceneManager) => {
      postFunctionSwitchInfo(
        initParams.getRequest(),
        sceneManager.getFunctionSwitches().getTypes(),
        initParams.getAppMeta()
      ).then((success) => {
        hilog.info(0x0000, TAG, `function type report: ${success}`);
      }).catch((e: Error) => {
        hilog.info(0x0000, TAG, `function type report err: ${e}`);
      })
    }, 10 * 1000, sceneManager);

    Btrace.sceneManager = sceneManager;
    Btrace.state = State.kInitialized;
    hilog.info(0x0000, TAG, 'init success');
    return true;
  }

  // Btrace 未启用时清理本地可能存在的未上报的 trace 文件
  static async cleanupOnDisable(context: Context) {
    let count = await taskpool.execute(cleanupLegacyEventTraces, context.cacheDir) as number;
    if (count > 0) {
      hilog.info(0x0000, TAG, '%{public}d legacy traces has been cleaned', count);
    }
  }

  static initialized(): boolean {
    return Btrace.state == State.kInitialized;
  }

  static async dumpTraceAndUpload(id: number, extraObj: Record<string, Object> | undefined = undefined,
                                  tids: number[]=[], beginTime: number=0, endTime: number=0): Promise<boolean> {
    if (Btrace.state != State.kInitialized) {
      return Promise.resolve(false);
    }

    if (!Btrace.sceneManager) {
      return Promise.resolve(false);
    }

    return await Btrace.sceneManager.dumpAndUpload(id, undefined, 0, extraObj, tids, beginTime, endTime) as boolean;
  }
}

function isValidOsVersion(osVersionLimit: string): boolean {
  if (osVersionLimit && 0 < osVersionLimit.length) {
    let osFullName = deviceInfo.osFullName;
    let index = osFullName.indexOf("-");

    if (index == -1 || index == osFullName.length-1) {
      return false;
    }

    let currOsVersion = osFullName.substring(index+1);

    // currOsVersion must be smaller than osVersionLimit
    if (compareVersion(currOsVersion, osVersionLimit) != -1) {
      return false;
    }
  }

  return true;
}

function compareVersion(v1: string, v2: string): number {
  const arr1 = v1.split('.').map(Number);
  const arr2 = v2.split('.').map(Number);
  const len = Math.max(arr1.length, arr2.length);

  for (let i = 0; i < len; i++) {
    const num1 = arr1[i] || 0; // 如果版本位数不够，补0
    const num2 = arr2[i] || 0;
    if (num1 > num2) return 1;
    if (num1 < num2) return -1;
  }
  return 0; // 相等
}