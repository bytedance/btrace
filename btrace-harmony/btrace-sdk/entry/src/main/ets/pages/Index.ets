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

import { BusinessError } from '@kit.BasicServicesKit'
import { http } from '@kit.NetworkKit'
import { Person } from '../aopCases/Person';
import { router } from '@kit.ArkUI';
import native from 'libentry.so';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { taskpool } from '@kit.ArkTS';
import { clearDir, testConcurrentObject } from '../utils/ConcurrentFunction';
import { Foo } from '../utils/Instances';
import { Btrace } from '@bytedance/btrace';

@Entry
@Component
struct Index {
  @State message: string = 'Hello World';
  LOG_TAG = "[SysAopTag] ";

  build() {
    Row() {
      Column() {
        Button() {
          Text('bluetooth').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          // let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
          // try {
          //   let context = getContext(this);
          //   atManager.requestPermissionsFromUser(context, ['ohos.permission.ACCESS_BLUETOOTH'],
          //     (err: BusinessError, data: PermissionRequestResult) => {
          //       if (access.getState() == access.BluetoothState.STATE_OFF) {
          //         access.enableBluetooth();
          //       }
          //     });
          // } catch (err) {
          //   console.log(this.LOG_TAG + "catch err " + JSON.stringify(err));
          // }
          hilog.info(0x0000, 'btrace:Entry', '%{public}s', 'bluetook button clicked');
        })
        .margin({
          top: 20
        })

        Button() {
          Text('http').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          let b1 = http.createHttp();
          console.log(this.LOG_TAG + "b1 is null : " + (b1 == undefined))
          b1.request(
            "https://www.huawei.com",
            {
              method: http.RequestMethod.POST,
              header: {
                'Content-Type': 'application/json'
              }
            }, (err: BusinessError, data: http.HttpResponse) => {
            if (!err) {
              console.log(this.LOG_TAG + 'http request: ' + JSON.stringify(data.result));
            } else {
              console.log(this.LOG_TAG + JSON.stringify(err));
              console.log((new Error()).stack)
            }
          });
        })
        .margin({
          top: 20
        })

        Button() {
          Text('person').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          const ret = new Person('xiao ming', 11).getInfo();
          console.log(this.LOG_TAG + "info: " + ret)
          let e = new Error();
          hilog.info(0x0000, 'btrace:Entry', `stacktrace: ${e.stack}`);
          // Btrace.capture();
        })
        .margin({
          top: 20
        })

        Button() {
          Text('跳转').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          router.pushUrl({ url: 'pages/Second' })
        })
        .margin({
          top: 20
        })

        Button() {
          Text('录音页面').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          router.pushUrl({ url: 'pages/AudioPage' })
        })
        .margin({
          top: 20
        })

        Button() {
          Text('malloc').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          native.testMalloc();
        })
        .margin({
          top: 20
        })

        Button() {
          Text('free').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          native.testFree();
        })
        .margin({
          top: 20
        })

        Button() {
          Text('mock app freeze').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() =>{
          setTimeout(() => {
            let t = Date.now();
            while (Date.now() - t <= 15000) {}
          }, 1000);
          hilog.info(0x0000, 'btrace:Entry', 'mock app freeze button clicked');
        })
        .margin({
          top: 20
        })

        Button() {
          Text('mock hang').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() =>{
          setTimeout(() => {
            let flag = true
            let dur = 0
            let t = Date.now();
            while (Date.now() - t <= 3*1000) {
              let currDur = Date.now() - t
              currDur = currDur / 1000

              if (dur < currDur) {
                if (!flag) {
                  continue
                } else {
                  flag = true
                  dur = currDur + 1;
                  hilog.info(0x0000, 'btrace:Entry', 'mock hang, sleep %d s', dur);
                }
              }
            }
          }, 100);
          hilog.info(0x0000, 'btrace:Entry', 'mock hang button clicked');
        })
        .margin({
          top: 20
        })

        Button() {
          Text('clean uploaded').fontSize(25).fontWeight(FontWeight.Bold)
        }
        .type(ButtonType.Capsule)
        .backgroundColor('#0D9FFB')
        .width(200)
        .height(50)
        .onClick(() => {
          let context = this.getUIContext().getHostContext()?.getApplicationContext();
          if (context) {
            let dir = context.cacheDir + '/uploaded';
            taskpool.execute(clearDir, dir);
          }
        })
        .margin({
          top: 20
        })

        // Button(){
        //   Text('dump symbols').fontSize(25).fontWeight(FontWeight.Bold);
        // }
        // .type(ButtonType.Capsule)
        // .backgroundColor('#0D9FFB')
        // .width(200)
        // .height(50)
        // .onClick(()=> {
        //   let context = this.getUIContext().getHostContext()?.getApplicationContext();
        //   Btrace.dumpSymbos(context!!);
        // })
        // .margin({
        //   top: 20
        // })

      } // column
      .width('100%')
    }
    .height('100%')
  }
}
