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

import { TraceConfig } from './TraceConfig';
import { SceneConfig } from './SceneConfig';
import { TraceAppMeta } from './TraceAppMeta';
import { Request } from './Request';
import { EventRegistry } from './EventRegistry';


export class BtraceInitParams {
  private traceConfig: TraceConfig;
  private sceneConfig: SceneConfig;
  private appMeta: TraceAppMeta;
  private request: Request;
  private eventRegistry: EventRegistry | undefined;
  private osVersionLimit: string;
  private appVersionCodeLimit: number;

  constructor(traceConfig: TraceConfig, sceneConfig: SceneConfig, appMeta: TraceAppMeta, request: Request,
              eventRegistry?: EventRegistry, osVersionLimit: string = "", appVersionCodeLimit: number = 0) {
    this.traceConfig = traceConfig;
    this.sceneConfig = sceneConfig;
    this.appMeta = appMeta;
    this.request = request;
    this.eventRegistry = eventRegistry;
    this.osVersionLimit = osVersionLimit;
    this.appVersionCodeLimit = appVersionCodeLimit;
  }

  getTraceConfig(): TraceConfig {
    return this.traceConfig;
  }

  getSceneConfig(): SceneConfig {
    return this.sceneConfig;
  }

  getAppMeta(): TraceAppMeta {
    return this.appMeta;
  }

  getRequest(): Request {
    return this.request;
  }

  getEventRegistry(): EventRegistry | undefined {
    return this.eventRegistry;
  }

  getOsVersionLimit(): string {
    return this.osVersionLimit;
  }

  getAppVersionCodeLimit(): number {
    return this.appVersionCodeLimit;
  }

}