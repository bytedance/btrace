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



import { BtraceOfflineInit, ClearDir, LocalHostHttpServer, dumpTrace } from './OfflineHelper';

import fs from '@ohos.file.fs';
import util from '@ohos.util';
import { application, UIAbility } from '@kit.AbilityKit';
import { BusinessError } from '@ohos.base';
import { AbilityLifecycleCallback } from '@kit.AbilityKit';
import window from '@ohos.window';
import { TraceAbility } from '../TraceAbility';
import { Btrace } from '../Btrace';

export class OfflineServer {

  private static started: boolean = false;
  private static initialized: boolean;
  private static workingDir: string;
  private static configFile: string = "/config.json";
  private static server?: LocalHostHttpServer;

  static Init(): void {

    let context = application.getApplicationContext();
    let bundleName = context.applicationInfo.name

    let offlineDir = context.cacheDir + `/ohtraceOffline`

    try {
      if (!fs.accessSync(offlineDir)) {
        fs.mkdirSync(offlineDir);
      }
    } catch (error) {
      // TODO: Implement error handling.
    }

    OfflineServer.workingDir = offlineDir

    if (OfflineServer.server) {
      OfflineServer.server.stopServer()
    }

    const server = new LocalHostHttpServer()
    OfflineServer.server = server
    OfflineServer.initServer(context, server)

    let port = OfflineServer.calculatePort(bundleName)
    OfflineServer.server.startServer(port).then((info) => {
      console.log(`btrace offline local server started on http://${info.address}:${info.port}`)
      // context.on('abilityLifecycle', {
      //   onAbilityDestroy(ability: UIAbility) {
      //     console.log('btrace offline onAbilityDestroy')
      //     OfflineServer.server?.stopServer()
      //     OfflineServer.server = undefined
      //   },
      //   onAbilityCreate: (ability: UIAbility): void => {},
      //   onWindowStageCreate: (ability: UIAbility, windowStage: window.WindowStage): void => {},
      //   onWindowStageActive: (ability: UIAbility, windowStage: window.WindowStage): void => {},
      //   onWindowStageInactive: (ability: UIAbility, windowStage: window.WindowStage): void => {},
      //   onWindowStageDestroy: (ability: UIAbility, windowStage: window.WindowStage): void => {},
      //   onAbilityForeground: (ability: UIAbility): void => {},
      //   onAbilityBackground: (ability: UIAbility): void => {},
      //   onAbilityContinue: (ability: UIAbility): void => {}
      // });
    }
    )

    let configPath = OfflineServer.workingDir + OfflineServer.configFile

    try {
      if (fs.accessSync(configPath)) {
        let jsonStr = fs.readTextSync(configPath)

        let config = JSON.parse(jsonStr) as Record<string, number>
        OfflineServer.Start(context, config);

        console.log(`btrace start at app launch.`)
      }
    } catch (error) {
      // TODO: Implement error handling.
    }
  }

  static Start(context: Context, config?: Record<string, number>): void {
    let bufferSize = config?.buffer_size as number
    let sampleInterval = config?.sample_interval as number
    let mainOnly = config?.main_only as number
    let highFreq = config?.high_freq as number
    let asyncInterval = config?.async_interval??0 as number
    let enableOsThread = config?.enable_os_thread??1 as number
    let syncMode = config?.sync_mode??0 as number
    let bufferedTime = config?.buffered_time??0 as number

    if (bufferSize <= 0) {
      bufferSize = 10000;
    }

    if (sampleInterval <= 0) {
      sampleInterval = 1;
    }

    if (asyncInterval < 0) {
      asyncInterval = 0;
    }

    ClearDir(OfflineServer.workingDir)

    if (!OfflineServer.initialized) {
      OfflineServer.initialized = true
      BtraceOfflineInit(context, bufferSize, sampleInterval, 0 < mainOnly, 0 < highFreq,
                asyncInterval, 0 < enableOsThread, 0 < syncMode, bufferedTime)
    } else {
      TraceAbility.resume();
    }

    OfflineServer.started = true;
  }

  static calculatePort(packageName: string): number  {
    const basePort = 11288
    const asciiSum: number = Array.from(packageName).reduce((sum, char) => sum + char.charCodeAt(0), 0);
    const portRange: number = 65536 - basePort;
    const resultPort: number = (asciiSum % portRange) + basePort;
    return resultPort;
  }

  static initServer(context: Context, server: LocalHostHttpServer): void  {
    server.auto()
    server.get('/', async(req, res) => {
      res.json({
        message: 'api list',
        availableRoutes: [
          'GET /',
          'POST /record/start',
          'POST /record/stop',
          'POST /config/upload'
        ],
        timestamp: new Date().toISOString()
      });
    });
    server.post('/record/start', async (req, res) => {
      let body = (req.body as Record<string, number>)

      OfflineServer.Start(context, body);

      res.status(200).json({})
    })
    server.post('/record/stop', async (req, res) => {
      let reqBody = (req.body as Record<string, number>)

      if (!OfflineServer.started) {
        res.status(500).json({
          msg: "Trace does not start"
        });
        return;
      }

      TraceAbility.pause()

      let traceDir = OfflineServer.workingDir
      let result = await dumpTrace(traceDir)

      if (!result) {
        let msg = "Failed to dump trace"

        if (!Btrace.initialized()) {
          msg += ", trace sdk is not initialized"
        } else {
          msg += ", unknown native error"
        }

        res.status(500).json({
          msg: msg
        });
        return;
      }

      let respBody: Record<string, string> = {}

      try {
        // 获取目录下的文件列表
        let fileList = await fs.listFile(traceDir);
        for (let i = 0; i < fileList.length; i++) {
          let fileName = fileList[i]
          let filePath = traceDir + '/' + fileName;
          let stat = fs.statSync(filePath);

          if (stat.isDirectory()) {
            continue
          }

          let file = fs.openSync(filePath, fs.OpenMode.READ_ONLY);

          // let stat = fs.statSync(file.fd);
          let buf = new ArrayBuffer(stat.size);
          fs.readSync(file.fd, buf);
          fs.closeSync(file);

          let base64Helper = new util.Base64Helper();
          let textUint8Array = new Uint8Array(buf);
          let encodedString = base64Helper.encodeToStringSync(textUint8Array);
          respBody[fileName] = encodedString
        }

        OfflineServer.started = false;
        res.status(200).json(respBody);
      } catch (e) {
        console.error(`Failed to gen resp. Code: ${e.code}, message: ${e.message}`);
        res.status(500).json({});
      }
    })
    server.post('/config/upload', async (req, res) => {
      let body = (req.body as Record<string, number>)

      let configPath = OfflineServer.workingDir + OfflineServer.configFile
      try {
        let file = fs.openSync(configPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
        let configStr = JSON.stringify(body)
        fs.writeSync(file.fd, configStr);
        fs.closeSync(file);
      } catch (error) {
        console.error('write config file error: ' + (error as BusinessError).message);
      }

      res.status(200).json({})
    })
  }
}
