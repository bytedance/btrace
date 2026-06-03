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



import { hilog } from '@kit.PerformanceAnalysisKit';
import fs from '@ohos.file.fs';
import { errorManager } from '@kit.AbilityKit';
import { taskpool, util } from '@kit.ArkTS';

import { HttpServer } from '@cxy/webserver';

import { OfflineAsyncDumpTrace } from './OfflineAsyncFunction';
import { TraceConfig } from '../TraceConfig';
import { SceneConfig } from '../SceneConfig';
import { BtraceInitParams } from '../BtraceInitParams';
import { TraceAppMeta } from '../TraceAppMeta';
import { Btrace } from '../Btrace';
import { DefaultEventRegistry } from '../DefaultEventRegistry';
import { Request } from '../Request';

@Sendable
class OfflineTraceAppMetaImpl implements TraceAppMeta {
  appId: string;
  deviceId: string;
  updateVersionCode: number;
  isBoe: boolean;
  buildTime: number;
  appLaunchTime: number;

  constructor(appId: string, deviceId: string, versionCode: number,
              isBoe: boolean, buildTime: number, launchTime: number) {
    this.appId = appId;
    this.deviceId = deviceId;
    this.updateVersionCode = versionCode;
    this.isBoe = isBoe;
    this.buildTime = buildTime;
    this.appLaunchTime = launchTime;
  }
}

@Sendable
class OfflineRequestImpl implements Request {

  private hostFileDir: string;

  constructor(hostFileDir: string) {
    this.hostFileDir = hostFileDir;
  }

  async postFile(host: string, path: string, file: [string, string],
                 bodyParams: Map<string, string>): Promise<boolean> {
    let p = '{';
    for (const it of bodyParams.entries()) {
      p += `[${it[0]}, ${it[1]}] `
    }
    p += '}'
    hilog.info(0x0000, 'btrace:post', `posting file ${file[1]} with params ${p} to ${host}${path}`);
    let filePath = file[1];
    let targetDir = this.hostFileDir + '/uploaded';
    let exist = await fs.access(targetDir);
    if (!exist) {
      await fs.mkdir(targetDir);
    }
    let dest = targetDir + filePath.substring(filePath.lastIndexOf('/'));
    hilog.info(0x0000, 'btrace:upload', `copying to ${dest}`);
    try {
      await fs.copyFile(filePath, dest);
    } catch (e) {
      hilog.info(0x0000, 'btrace:upload', `copy error: ${e}`);
      return false;
    }
    hilog.info(0x0000, 'btrace:upload', `copied to ${dest}`);
    return true;
  }

  async postJson(host: string, path: string, json: string): Promise<boolean> {
    hilog.info(0x0000, 'btrace:', `posting json ${json} to ${host}${path}`);

    return true;
  }
}

class OfflineEventRegistryImpl extends DefaultEventRegistry {
}

export function BtraceOfflineInit(context: Context, bufferSize: number,
                          sampleInterval: number, mainOnly: boolean,
                          highFreq: boolean, asyncInterval: number,
                          enableOsThread: boolean, syncMode: boolean,
                          bufferedTime: number): void {
  let bgSampleInterval = 2 * sampleInterval
  let bgAsyncInterval = 2 * asyncInterval

  if (mainOnly) {
    bgSampleInterval = 0
    bgAsyncInterval = 0
  }

  let disableSignal: boolean = true

  if (0 < asyncInterval) { disableSignal = false; }

  let traceConfig = new TraceConfig();
  traceConfig.setEnabled(true);
  traceConfig.setBufferSize(bufferSize);
  traceConfig.setSampleInterval(sampleInterval, bgSampleInterval);
  traceConfig.setDisableSignalBacktrace(disableSignal);
  traceConfig.setAsyncSampleInterval(asyncInterval, bgAsyncInterval);
  traceConfig.setDisableCustomUnwind(false);
  traceConfig.setForceLoad(true);
  traceConfig.setHighFreq(highFreq);
  traceConfig.setEnableOsThread(enableOsThread);
  traceConfig.setDisableSigMask(false);
  traceConfig.setBufferedTime(bufferedTime);
  traceConfig.setEnablePreload(true);
  traceConfig.setParseElfSymbols(true);
  let sceneConfig = new SceneConfig();
  sceneConfig.setEnable(true);
  sceneConfig.setAppFreezeEnable(true);
  sceneConfig.setCustomAppFreezeMonitor(true);
  // sceneConfig.setFrameDropEnable(true);
  // sceneConfig.setFrameDropThresMs(1000);
  let appMeta = new OfflineTraceAppMetaImpl('1', 'test_did', 1, false, 0, Date.now());
  let initParams = new BtraceInitParams(traceConfig, sceneConfig, appMeta, new OfflineRequestImpl(context.filesDir),
                                        new OfflineEventRegistryImpl());
  try {
    if (syncMode) {
      Btrace.syncInit(context, initParams);
    } else {
      Btrace.init(context, initParams);
    }
  } catch (e) {
    hilog.info(0x0000, 'btrace:init', 'Btrace init failed: %{public}s', e);
  }
}

export function ClearDir(targetDir: string): void {
  try {
    let fileList = fs.listFileSync(targetDir);
    for (let i = 0; i < fileList.length; i++) {
      let fileName = fileList[i]
      let filePath = targetDir + '/' + fileName;
      let stat = fs.statSync(filePath);

      if (stat.isDirectory()) {
        fs.rmdirSync(filePath);
      } else {
        fs.unlinkSync(filePath);
      }
    }
  } catch (e) {
    console.error(`Failed to clear directory. Code: ${e.code}, message: ${e.message}`);
  }
}

export class LocalHostHttpServer extends HttpServer {
  protected getLocalIP(): string {
    return '127.0.0.1'
  }
}

export async function dumpTrace(targetDir: string): Promise<boolean> {
  if (!Btrace.initialized()) {
    return Promise.resolve(false);
  }

  return await taskpool.execute(OfflineAsyncDumpTrace, targetDir) as boolean;
}