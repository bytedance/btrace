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

import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { SceneConfig, TraceConfig, Btrace, TraceAppMeta } from '@bytedance/btrace';
import { BtraceInitParams } from '@bytedance/btrace';
import { RequestImpl } from './RequestImpl';
import { EventRegistryImpl } from './EventRegistryImpl';
import { AppMetaImpl } from './AppMetaImpl';
import { OfflineServer } from '@bytedance/btrace';


function btraceInit(context: Context, enable: boolean): void {
  if (!enable) {
    Btrace.cleanupOnDisable(context);
    return;
  }
  let traceConfig = new TraceConfig();
  traceConfig.setEnabled(true);
  traceConfig.setBufferSize(32768);
  traceConfig.setSampleInterval(10, 20);
  traceConfig.setDisableSignalBacktrace(true);
  traceConfig.setDisableCustomUnwind(false);
  let sceneConfig = new SceneConfig();
  sceneConfig.setEnable(true);
  sceneConfig.setAppFreezeEnable(true);
  sceneConfig.setCustomAppFreezeMonitor(true);
  let appMeta: TraceAppMeta = new AppMetaImpl('1', 'test_did', 1, false, 0, Date.now());
  let initParams = new BtraceInitParams(traceConfig, sceneConfig, appMeta, new RequestImpl(context.getApplicationContext().filesDir), new EventRegistryImpl());
  try {
    Btrace.init(context, initParams);
  } catch (e) {
    hilog.info(0x0000, 'btrace:init', 'Btrace init failed: %{public}s', e);
  }
}

const TAG = 'btrace:demo_ability';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    try {
      OfflineServer.Init()
    } catch (e) {
      hilog.info(0x0000, 'btrace:init', 'Btrace init failed: %{public}s', e);
    }
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onCreate');
  }


  onDestroy(): void {
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(0x0000, TAG, 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, TAG, 'Succeeded in loading the content.');
    });
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(0x0000, TAG, '%{public}s', 'Ability onBackground');
  }
};