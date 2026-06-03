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

//
// Created on 2025/10/9.
//

#include "TraceConfigurations.h"
#include <cstdint>
#include <hilog/log.h>

namespace btrace {

TraceConfigurations& TraceConfigurations::get() {
    static TraceConfigurations inst{};
    return inst;
}

void TraceConfigurations::update(napi_env env, napi_value configArray, uint32_t length) {
    if (length != 16) {
        OH_LOG_ERROR(LOG_APP, "trace configuration array length error, expected: 4, actual: %{public}d", length);
        return;
    }
    napi_value first = nullptr;
    int32_t firstVal = 0;
    napi_get_element(env, configArray, 0, &first);
    napi_get_value_int32(env, first, &firstVal);
    napi_value second = nullptr;
    int32_t secondVal = 0;
    napi_get_element(env, configArray, 1, &second);
    napi_get_value_int32(env, second, &secondVal);
    napi_value third = nullptr;
    int32_t thirdVal = 0;
    napi_get_element(env, configArray, 2, &third);
    napi_get_value_int32(env, third, &thirdVal);
    napi_value fourth = nullptr;
    int32_t fourthVal = 0;
    napi_get_element(env, configArray, 3, &fourth);
    napi_get_value_int32(env, fourth, &fourthVal);
    napi_value fifth = nullptr;
    int32_t fifthVal = 0;
    napi_get_element(env, configArray, 4, &fifth);
    napi_get_value_int32(env, fifth, &fifthVal);
    napi_value sixth = nullptr;
    int32_t sixthVal = 0;
    napi_get_element(env, configArray, 5, &sixth);
    napi_get_value_int32(env, sixth, &sixthVal);
    napi_value seventh = nullptr;
    int32_t seventhVal = 0;
    napi_get_element(env, configArray, 6, &seventh);
    napi_get_value_int32(env, seventh, &seventhVal);
    napi_value eighth = nullptr;
    int32_t eighthVal = 0;
    napi_get_element(env, configArray, 7, &eighth);
    napi_get_value_int32(env, eighth, &eighthVal);
    napi_value ninth = nullptr;
    int32_t ninthVal = 0;
    napi_get_element(env, configArray, 8, &ninth);
    napi_get_value_int32(env, ninth, &ninthVal);
    napi_value tenth = nullptr;
    int32_t tenthVal = 0;
    napi_get_element(env, configArray, 9, &tenth);
    napi_get_value_int32(env, tenth, &tenthVal);
    napi_value eleventh = nullptr;
    int32_t eleventhVal = 0;
    napi_get_element(env, configArray, 10, &eleventh);
    napi_get_value_int32(env, eleventh, &eleventhVal);
    napi_value twelfth = nullptr;
    int32_t twelfthVal = 0;
    napi_get_element(env, configArray, 11, &twelfth);
    napi_get_value_int32(env, twelfth, &twelfthVal);
    napi_value thirteenth = nullptr;
    int32_t thirteenthVal = 0;
    napi_get_element(env, configArray, 12, &thirteenth);
    napi_get_value_int32(env, thirteenth, &thirteenthVal);
    napi_value fourteenth = nullptr;
    int32_t fourteenthVal = 0;
    napi_get_element(env, configArray, 13, &fourteenth);
    napi_get_value_int32(env, fourteenth, &fourteenthVal);
    napi_value fifteenth = nullptr;
    int32_t fifteenthVal = 0;
    napi_get_element(env, configArray, 14, &fifteenth);
    napi_get_value_int32(env, fifteenth, &fifteenthVal);
    napi_value sixteenth = nullptr;
    int32_t sixteenthVal = 0;
    napi_get_element(env, configArray, 15, &sixteenth);
    napi_get_value_int32(env, sixteenth, &sixteenthVal);
    enabled = firstVal > 0;
    bufferConfig.bufferSize = secondVal > 0 ? secondVal : 0;
    unwindConfig.mainThreadSampleInterval = thirdVal > 0 ? thirdVal : 0;
    unwindConfig.subThreadSampleInterval = fourthVal > 0 ? fourthVal : 0;
    unwindConfig.disableSignalBacktrace = fifthVal == 1;
    unwindConfig.disableCustomUnwind = sixthVal == 1;
    unwindConfig.forceLoad = seventhVal == 1;
    hookConfig.magic = eighthVal;
    unwindConfig.highFreq = ninthVal == 1;
    unwindConfig.mainAsyncInterval = tenthVal > 0 ? tenthVal : 0;
    unwindConfig.childAsyncInterval = eleventhVal > 0 ? eleventhVal : 0;
    unwindConfig.enableOsThread = twelfthVal == 1;
    unwindConfig.bufferedTime = thirteenthVal > 0 ? thirteenthVal : 0;
    unwindConfig.disableSigMask = fourteenthVal == 1;
    bufferConfig.enablePreload = fifteenthVal == 1;
    unwindConfig.parseElfSymbols = sixteenthVal == 1;
}

}