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

#include <jni.h>
#include <vector>

//
// Created by ByteDance on 5/27/25.
//

void performMemoryStress(JNIEnv *env, int loopCount, int blockSize) {
    volatile char *temp; // volatile防止编译器优化

    for (int i = 0; i < loopCount; ++i) {
        // 分配大块内存
        char *buffer = new char[blockSize];

        // 写入数据防止优化
        for (int j = 0; j < blockSize; ++j) {
            buffer[j] = j % 256;
        }

        // 记录中间值（强制编译器保留操作）
        temp = buffer;

        // 释放内存
        delete[] buffer;

        // 制造内存碎片（可选）
        if (i % 100 == 0) {
            std::vector<char *> fragments;
            for (int k = 0; k < 50; ++k) {
                fragments.push_back(new char[blockSize / 2]);
            }
            for (auto frag: fragments) {
                delete[] frag;
            }
            auto cls = env->FindClass("sample/android/jarvis/MainActivity");
            auto sleep = env->GetStaticMethodID(cls, "sleepFromJNI", "()V");
            env->CallStaticVoidMethod(cls, sleep);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_sample_android_jarvis_MainActivity_nativeTest(JNIEnv *env, jclass clazz) {
    performMemoryStress(env, 500, 1024000);
}