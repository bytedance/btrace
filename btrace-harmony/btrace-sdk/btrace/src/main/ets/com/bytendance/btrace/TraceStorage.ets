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

import fs from '@ohos.file.fs';
import { TraceEventFile } from './TraceEventFile';

@Sendable
export class TraceStorage {

  baseDir: string;

  constructor(baseDir: string) {
    this.baseDir = baseDir;
  }

  static makePhantom(appFileDir: string): TraceStorage {
    let baseDir = appFileDir + '/btrace';
    return new TraceStorage(baseDir);
  }

  static init(appFileDir: string): TraceStorage {
    let baseDir = appFileDir + '/btrace';
    if (!fs.accessSync(baseDir)) {
      fs.mkdirSync(baseDir);
    }
    return new TraceStorage(baseDir);
  }

  getBaseDir(): string {
    return this.baseDir;
  }

  makeAppFreezeDir(time: number): TraceEventFile {
    return this.makeTraceFile(200, time);
  }

  cleanupTraceFile(traceEventFile: TraceEventFile) {
    if (fs.accessSync(traceEventFile.dir)) {
      fs.rmdirSync(traceEventFile.dir);
    }
    if (traceEventFile.zip.length > 0 && fs.accessSync(traceEventFile.zip)) {
      fs.unlinkSync(traceEventFile.zip);
    }
  }

  async asyncCleanupTraceFile(traceEventFile: TraceEventFile) {
    try {
      if (await fs.access(traceEventFile.dir)) {
        await fs.rmdir(traceEventFile.dir);
      }

      if (0 < traceEventFile.zip.length && await fs.access(traceEventFile.zip)) {
        await fs.unlink(traceEventFile.zip);
      }
    } catch (e) {
    }
  }

  listTraceFiles(): TraceEventFile[] {
    let files = fs.listFileSync(this.baseDir);
    if (files.length == 0) {
      return [];
    }

    let dirs = new Map<string, TraceEventFile>();
    for (let name of files) {
      let fullPath = this.baseDir + '/' + name;
      let stat = fs.statSync(fullPath);
      if (stat.isDirectory()) {
        let traceFile = this.makeTraceFileForName(name)
        if (!traceFile) { continue; }
        dirs.set(name, traceFile);
      }
    }

    return Array.from(dirs.values());
  }

  makeTraceFileForName(name: string): TraceEventFile | undefined {
    let underlinePos = name.lastIndexOf('_')
    let sceneIdStr = name.substring(0, underlinePos)
    let timeStr = name.substring(underlinePos+1)
    let sceneId = Number.parseInt(sceneIdStr);
    let time = Number.parseInt(timeStr);

    if (Number.isNaN(sceneId) || Number.isNaN(time)) {
      return undefined
    }

    return this.makeTraceFile(sceneId, time);
  }

  makeTraceFile(id: number, time:number): TraceEventFile {
    let dir = this.baseDir + `/${id}_${time}`
    if (!fs.accessSync(dir)) {
      fs.mkdirSync(dir);
    }
    let zip = dir + '.zip';
    if (fs.accessSync(zip)) {
      return new TraceEventFile(id, time, dir, zip);
    }
    return new TraceEventFile(id, time, dir);
  }
}