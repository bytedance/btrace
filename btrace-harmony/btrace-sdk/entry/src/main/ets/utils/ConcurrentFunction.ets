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

import systemDateTime from '@ohos.systemDateTime'
import { hilog } from '@kit.PerformanceAnalysisKit';
import { Foo } from './Instances'
import fs from '@ohos.file.fs';

@Concurrent
export function testReturnBoolean(): boolean {
  let time = systemDateTime.getTime();
  hilog.info(0x0000, 'btrace:async', `got time: ${time}`);
  return time % 2 === 0;
}

@Concurrent
export function testConcurrentObject(foo: Foo): void {
  hilog.info(0x0000, 'btrace:async', 'got foo(%{public}d, %{public}s)', foo.count, foo.desc);
  foo.inc();
  hilog.info(0x0000, 'btrace:async', 'inc foo(%{public}d, %{public}s)', foo.count, foo.desc);
}

@Concurrent
export function clearDir(dir: string) {
  let exist = fs.accessSync(dir);
  if (!exist) {
    hilog.info(0x0000, 'btrace:async', `dir not exist: ${dir}`);
    return;
  }
  let fileNames = fs.listFileSync(dir);
  if (fileNames.length === 0) {
    hilog.info(0x0000, 'btrace:async', `empty dir: ${dir}`);
    return;
  }
  for (let name of fileNames) {
    let path = dir + '/' + name;
    fs.unlinkSync(path);
    hilog.info(0x0000, 'btrace:async', `file ${path} removed`);
  }
}