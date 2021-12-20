package rhea.sample.android.app

import android.app.Application
import android.content.Context
import android.util.Log

class RheaApplication : Application() {

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
    }

    override fun onCreate() {
        super.onCreate()
        printApplicationName(this.javaClass.name)
    }

    private fun printApplicationName(appName: String) {
        Log.d("RheaTrace", appName)
    }
}