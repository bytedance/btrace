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

import { Request } from '@bytedance/btrace';
import { hilog } from '@kit.PerformanceAnalysisKit';
import fs from '@ohos.file.fs';

@Sendable
export class RequestImpl implements Request {

  private hostFileDir: string;

  constructor(hostFileDir: string) {
    this.hostFileDir = hostFileDir;
  }

  async postFile(host: string, path: string, file: [string, string], bodyParams: Map<string, string>): Promise<boolean> {
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