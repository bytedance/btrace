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
import { zlib } from "@kit.BasicServicesKit";
import { JSON, process } from "@kit.ArkTS";
import { Request } from "./Request"
import { TraceEventFile } from "./TraceEventFile";
import { postTraceFile } from "./RequestHelper"

interface AnrParams {
  app_id: string;
  device_id: string;
  scene_id: number;
  current_time_ms: number;
  build_time: number;
  app_boottime_ms: number;
}

interface CprfParams {
  appId: number;
  deviceId: string;
  sceneId: number;
  currentTimeMs: number;
  buildTime: number;
  appBoottimeMs: number;
}


@Sendable
export class TraceUploader {

  static instance: TraceUploader | undefined;

  private appMeta: TraceAppMeta;
  private request: Request;

  constructor(appMeta: TraceAppMeta, request: Request) {
    this.appMeta = appMeta;
    this.request = request;
  }

  static init(appMeta: TraceAppMeta, request: Request): TraceUploader {
    if (TraceUploader.instance) {
      return TraceUploader.instance;
    }
    TraceUploader.instance = new TraceUploader(appMeta, request);
    return TraceUploader.instance;
  }

  getAppMeta(): TraceAppMeta {
    return this.appMeta;
  }

  async zip(dir: string): Promise<string> {
    let outFile = dir +  '.zip';
    let options: zlib.Options = {
      level: zlib.CompressLevel.COMPRESS_LEVEL_DEFAULT_COMPRESSION,
      memLevel: zlib.MemLevel.MEM_LEVEL_DEFAULT,
      strategy: zlib.CompressStrategy.COMPRESS_STRATEGY_DEFAULT_STRATEGY
    };
    return zlib.compressFile(dir, outFile, options).then(() => {
      return outFile;
    })
  }

  upload(sceneId: number, traceEventFile: TraceEventFile): Promise<TraceEventFile> {
    if (traceEventFile.zip.length > 0) {
      let paramsJSONStr = ""

      if (sceneId == 200) {
        let anrParams: AnrParams = {
          app_id: this.appMeta.appId,
          device_id: this.appMeta.deviceId,
          scene_id: sceneId,
          current_time_ms: traceEventFile.time,
          build_time: this.appMeta.buildTime,
          app_boottime_ms: this.appMeta.appLaunchTime
        }
        paramsJSONStr = JSON.stringify(anrParams)
      } else {
        let appId = Number.parseInt(this.appMeta.appId);

        if (Number.isNaN(appId)) {
          appId = 0
        }

        let cprfParams: CprfParams = {
          appId: appId,
          deviceId: this.appMeta.deviceId,
          sceneId: sceneId,
          currentTimeMs: traceEventFile.time,
          buildTime: this.appMeta.buildTime,
          appBoottimeMs: this.appMeta.appLaunchTime
        }
        paramsJSONStr = JSON.stringify(cprfParams)
      }

      const paramMap = new Map<string, string>();
      paramMap.set('params', paramsJSONStr);
      paramMap.set('anr_data','');

      return postTraceFile(this.request, sceneId, ['zip_file', traceEventFile.zip], paramMap, this.appMeta).then((success: boolean) => {
        if (success) {
          return traceEventFile;
        }
        throw new Error('upload failed');
      })
    }

    let inDir = traceEventFile.dir;
    let outFile = inDir +  '.zip';
    let options: zlib.Options = {
      level: zlib.CompressLevel.COMPRESS_LEVEL_DEFAULT_COMPRESSION,
      memLevel: zlib.MemLevel.MEM_LEVEL_DEFAULT,
      strategy: zlib.CompressStrategy.COMPRESS_STRATEGY_DEFAULT_STRATEGY
    };
    return zlib.compressFile(inDir, outFile, options).then(() => {
      traceEventFile.zip = outFile;

      let paramsJSONStr = ""

      if (sceneId == 200) {
        let anrParams: AnrParams = {
          app_id: this.appMeta.appId,
          device_id: this.appMeta.deviceId,
          scene_id: sceneId,
          current_time_ms: traceEventFile.time,
          build_time: this.appMeta.buildTime,
          app_boottime_ms: this.appMeta.appLaunchTime
        }
        paramsJSONStr = JSON.stringify(anrParams)
      } else {
        let appId = Number.parseInt(this.appMeta.appId);

        if (Number.isNaN(appId)) {
          appId = 0
        }
        let cprfParams: CprfParams = {
          appId: appId,
          deviceId: this.appMeta.deviceId,
          sceneId: sceneId,
          currentTimeMs: traceEventFile.time,
          buildTime: this.appMeta.buildTime,
          appBoottimeMs: this.appMeta.appLaunchTime
        }
        paramsJSONStr = JSON.stringify(cprfParams)
      }
      const paramMap = new Map<string, string>();
      paramMap.set('params', paramsJSONStr);
      paramMap.set('anr_data','');
      return postTraceFile(this.request, sceneId, ['zip_file', outFile], paramMap, this.appMeta)
    }).then((success) => {
      return traceEventFile;
    })
  }

  static assign(target: Record<string, Object>, ...source: Object[]): Record<string, Object> {
    for (const items of source) {
      for (const key of Object.keys(items)) {
        target[key] = Reflect.get(items, key)
      }
    }
    return target;
  }

  makeExtra(sceneId: number, currentTimeMs: number, customExtra?: Record<string, Object>): string | undefined {
    let extraObj: Record<string, Object> = {
      "currentTimeMs": currentTimeMs,
      "sceneId": sceneId,
      "processId": process.pid,
      "appBootTimeMs": this.appMeta.appLaunchTime,
      "updateVersionCod": this.appMeta.updateVersionCode
    };

    if (customExtra) {
      TraceUploader.assign(extraObj, customExtra);
    }

    let jsonStr: string | undefined = undefined
    try {
      jsonStr = JSON.stringify(extraObj);
    } catch (e) {
      jsonStr = undefined;
    }

    return jsonStr;
  }

}