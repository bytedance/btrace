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

import { TraceConfig } from "./TraceConfig";
import native from 'libbtrace.so'
import { hilog } from "@kit.PerformanceAnalysisKit";

const TAG = 'btrace:ability';

export class TraceAbility {
  static init(traceConfig: TraceConfig): boolean {
    if (!traceConfig.isEnabled()) {
      return false;
    }
    if (!native.initTracing(traceConfig.encode())) {
      hilog.error(0x0000, TAG, 'native init tracing failed')
      return false;
    }
    return true;
  }

  static resume() {
    native.resume();
  }

  static pause() {
    native.pause();
  }

  static dumpTrace(path: string, extra: string, tids: number[]=[], beginTimeMs: number=0, endTimeMs: number=0): boolean {
    return native.dumpTrace(path, extra, tids, beginTimeMs, endTimeMs);
  }

  static terminate() {
    native.terminateTracing();
  }
}