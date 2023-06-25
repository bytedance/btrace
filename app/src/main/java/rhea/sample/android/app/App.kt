/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package rhea.sample.android.app

import android.app.Application
import android.content.Context
import android.util.Log
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors

class App : Application() {
    private val executor = Executors.newCachedThreadPool()
    private val latch = CountDownLatch(4)

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        initSdks()
        latch.await()
    }

    private fun initSdks() {
        initCrashSdk()
        initImageLoaderAsync()
        initVideoDecoderAsyc()
        initVideoPlayerAsync()
        initRouterAsync()
        initNetwork()
    }

    private fun initRouterAsync() {
        executor.execute {
            fibonacci(12)
            latch.countDown()
        }
    }

    private fun initVideoPlayerAsync() {
        executor.execute {
            fibonacci(15)
            latch.countDown()
        }
    }

    private fun initVideoDecoderAsyc() {
        executor.execute {
            fibonacci(14)
            latch.countDown()
        }
    }

    private fun initImageLoaderAsync() {
        executor.execute {
            fibonacci(20)
            latch.countDown()
        }
    }

    private fun initNetwork() {
        println(fibonacci(10))
    }

    private fun initCrashSdk() {
        println(fibonacci(12))
    }

    private fun fibonacci(n: Int): Int {
        if (n == 0) return 0
        return if (n == 1) 1 else fibonacci(n - 1) + fibonacci(n - 2)
    }

    override fun onCreate() {
        super.onCreate()
        printApplicationName(this.javaClass.name)
    }

    private fun printApplicationName(appName: String) {
        Log.d("RheaTrace", appName)
    }
}