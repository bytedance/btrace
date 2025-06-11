package rhea.sample.android

import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.bytedance.rheatrace.core.TraceStub

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest {

    companion object {
        const val TAG: String = "Rhea:AndroidTest"
    }

    @Test
    fun useAppContext() {
        // Context of the app under test.
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("rhea.sample.android", appContext.packageName)
    }

    @Test
     fun testTraceStub() {
        val testCounts = 10
        val loopCount = 1000000

        val ids: Array<String?> = arrayOfNulls(loopCount)
        for (i in 0 until loopCount) {
            ids[i] = i.toString()
        }
        for (time in 0 until testCounts) {
            val start = System.currentTimeMillis()
            for (id in ids) {
                TraceStub.i(id)
                TraceStub.o(id)
            }
            Log.d(TAG, "unit test cost: " + (System.currentTimeMillis() - start) + "ms")
        }
    }
}