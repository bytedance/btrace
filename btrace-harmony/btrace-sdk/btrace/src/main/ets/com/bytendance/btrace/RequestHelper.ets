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

import { Request } from './Request';
import { TraceAppMeta } from './TraceAppMeta';

const RELEASE_HOST: string = 'scalpel-perf.zijieapi.com';
const BOE_HOST: string = 'scalpel-perf.bytedance.net';

export function postTraceFile(request: Request, sceneId: number, file: [string, string], bodyParams: Map<string, string>, meta: TraceAppMeta): Promise<boolean> {
  if (sceneId == 200) {
    return postAnrTraceFile(request, file, bodyParams, meta)
  }

  return postGeneralTraceFile(request, file, bodyParams, meta)
}

export function postAnrTraceFile(request: Request, file: [string, string], bodyParams: Map<string, string>, meta: TraceAppMeta): Promise<boolean> {
  let host = RELEASE_HOST
  let api = "/explore/api/v1/anr"

  if (meta.isBoe) {
    host = BOE_HOST
  }

  return request.postFile(host, api, file, bodyParams);
}

export function postGeneralTraceFile(request: Request, file: [string, string], bodyParams: Map<string, string>, meta: TraceAppMeta): Promise<boolean> {
  let host = RELEASE_HOST
  let api = "/explore/api/v1/cprf"

  if (meta.isBoe) {
    host = BOE_HOST
  }

  return request.postFile(host, api, file, bodyParams);
}

export function postFunctionSwitchInfo(request: Request, switches: number[], meta: TraceAppMeta): Promise<boolean> {
  let json: string = JSON.stringify({
    explore_type: switches,
    apiVersion: 1,
    appLaunchTime: meta.appLaunchTime
  })
  if (meta.isBoe) {
    return request.postJson(BOE_HOST, '/metrics/api/v1/explore', json);
  } else {
    return request.postJson(RELEASE_HOST, '/metrics/api/v1/explore', json);
  }
}