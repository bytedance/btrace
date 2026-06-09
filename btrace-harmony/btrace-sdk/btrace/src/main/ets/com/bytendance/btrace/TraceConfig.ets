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

@Sendable
export class TraceConfig {
  private enable: boolean = false;
  private bufferSize: number = 0;
  private mainThreadSampleInterval: number = 0;
  private subThreadSampleInterval: number = 0;
  private disableSignalBacktrace: boolean = false;
  private disableCustomUnwind: boolean = false;
  private forceLoad: boolean = false;
  private hookMagic: number = 0;
  private highFreq: boolean = false;
  private mainAsyncInterval: number = 0;
  private childAsyncInterval: number = 0;
  private enableOsThread: boolean = false;
  private bufferedTime: number = 0;
  private disableSigMask: boolean = false;
  private enablePreload: boolean = false;
  private parseElfSymbols: boolean = false;

  setEnabled(enable: boolean) {
    this.enable = enable;
  }

  isEnabled(): boolean {
    return this.enable;
  }

  setBufferSize(size: number) {
    this.bufferSize = size;
  }

  setSampleInterval(mainThreadInterval: number, subThreadInterval: number) {
    this.mainThreadSampleInterval = mainThreadInterval;
    this.subThreadSampleInterval = subThreadInterval;
  }

  setAsyncSampleInterval(mainAsyncInterval: number, childAsyncInterval: number) {
    this.mainAsyncInterval = mainAsyncInterval;
    this.childAsyncInterval = childAsyncInterval;
  }

  setDisableSignalBacktrace(disable: boolean) {
    this.disableSignalBacktrace = disable;
  }

  setDisableCustomUnwind(disable: boolean) {
    this.disableCustomUnwind = disable;
  }

  setForceLoad(disable: boolean) {
    this.forceLoad = disable;
  }

  setHighFreq(flag: boolean) {
    this.highFreq = flag;
  }

  setEnableOsThread(enable: boolean) {
    this.enableOsThread = enable
  }

  setDisableSigMask(disable: boolean) {
    this.disableSigMask = disable;
  }

  setBufferedTime(time: number) {
    this.bufferedTime = time;
  }

  setEnablePreload(enable: boolean) {
    this.enablePreload = enable
  }

  setHookMagic(magic: number) {
    this.hookMagic = magic;
  }

  setParseElfSymbols(parse: boolean) {
    this.parseElfSymbols = parse;
  }


  encode(): number[] {
    let a: number[] = [];
    a.push(this.enable ? 1 : 0);
    a.push(this.bufferSize);
    a.push(this.mainThreadSampleInterval);
    a.push(this.subThreadSampleInterval);
    a.push(this.disableSignalBacktrace ? 1 : 0);
    a.push(this.disableCustomUnwind ? 1 : 0);
    a.push(this.forceLoad ? 1 : 0);
    a.push(this.hookMagic);
    a.push(this.highFreq? 1 : 0);
    a.push(this.mainAsyncInterval);
    a.push(this.childAsyncInterval);
    a.push(this.enableOsThread? 1 : 0);
    a.push(this.bufferedTime)
    a.push(this.disableSigMask? 1 : 0);
    a.push(this.enablePreload? 1 : 0);
    a.push(this.parseElfSymbols? 1 : 0);
    return a;
  }
}