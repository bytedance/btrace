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

package sample.android.jarvis.test

import android.os.Build
import android.os.SystemClock
import androidx.annotation.Keep
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.FutureTask

/**
 * Created by majun.oreo on 2022/12/2
 * @author majun.oreo@bytedance.com
 */
@Keep
object FutureTest {
    fun test() {
        val thread = Executors.newFixedThreadPool(3)
        val tasks: MutableList<Future<Long>> = ArrayList()
        for (i in 0..9) {
            val task = FutureTask(Callable {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
                    val now = SystemClock.elapsedRealtimeNanos()
                    Thread.sleep((Math.random() * 100).toLong())
                    return@Callable SystemClock.elapsedRealtimeNanos() - now
                }
                0L
            })
            thread.execute(task)
            tasks.add(task)
        }
        try {
            for (task in tasks) {
                val aLong = task.get()
                System.out.println(aLong)
            }
        } catch (e: Throwable) {
            throw RuntimeException(e)
        }
    }
}