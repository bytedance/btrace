## btrace

[README 中文版](./README.zh-CN.md)

![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-1.0.1-red.svg?style=flat)

btrace(AKA RheaTrace) is a high performance Android trace tool which is based on [Systrace](https://developer.android.com/topic/performance/tracing), it support to [define custom events](https://developer.android.com/topic/performance/tracing/custom-events) automatically during building apk and using [bhook](https://github.com/bytedance/bhook) to provider more native events like IO.

## Key features
Support to define custom events automatically, inject **Trace#beginSection(String)** and **Trace#endSection()** into app methods during building apk.

Provider extra native events like IO to determine time-consuming reasons for methods conveniently.

Support to only capture trace events of main thread on demand.

Easily use, high stability, lower latency than systrace.

## Getting started
Import rhea-gradle-plugin as a dependency in your main build.gradle in the root of your project.

```
buildscript {
    repositories {
        ...
        mavenCentral()
        ...
    }
    dependencies {
        classpath 'com.bytedance.btrace:rhea-gradle-plugin:1.0.1'
    }
}

allprojects {
    repositories {
        ...
        mavenCentral()
        ...
    }
}
```

Then "apply" the plugin and import dependencies by adding the following lines to your app/build.gradle.

```
dependencies {
    //rheatrace core lib
    implementation "com.bytedance.btrace:rhea-core:1.0.1"
}
...
rheaTrace {

   compilation {
      //for reducing apk size, you can use method id to trace custom events, default false.
      traceWithMethodID = false 
      //decide which methods you don't want trace, default null.
      traceFilterFilePath = "${project.rootDir}/rhea-trace/traceFilter.txt"
      //keep name of custom event with specific method id, default null.
      applyMethodMappingFilePath = "${project.rootDir}/rhea-trace/keep-method-id.txt"
  }

   runtime {
      //capture trace events of main thread only, default false.
      mainThreadOnly true 
      //start to capture trace events when launch app, default true.
      startWhenAppLaunch true
      //set a specified size of ring buffer which used to store atrace data in memory.
      atraceBufferSize "500000"
   }
}
...
apply plugin: 'com.bytedance.rhea-trace'
```
About `rheaTrace `, Learn more from the [RheaTrace Gradle Config](./GRADLE_CONFIG.zh-CN.MD).

At last,  check python version in your computer, RheaTrace only support python 2.7 because of systrace, and then config systrace environment into ***~/.bash_profile*** file.

```
export PATH=${PATH}:/Users/${user_name}/Library/Android/sdk/platform-tools/systrace
```
## Usage

Open terminal, change directory to **RheaTrace/scripts/python/rheatrace**, execute command below.

```
python rheatrace.py -v
```
Then you will see output below.

```
Current version is 1.0.1.
```
RheaTrace retains the [Systrace command](https://developer.android.com/topic/performance/tracing/command-line) parameters, if you have used Systrace before, you will quickly learn how to use RheaTrace.

### Syntax
To generate the HTML report for app, you need to run `rheatrace` from the command line using the following syntax:

```
python rheatrace.py [options] [categories]
```
For example, the following command calls `rheatrace` to record device activity and generate a HTML report named mynewtrace.html. This list of categories is a reasonable default list for most devices.

```
python rheatrace.py -a rhea.sample.android -t 5 -o ./output/mynewtrace.html sched freq idle am wm gfx view binder_driver hal dalvik camera input res
```

### Global options
|  Global options   | Description |
|  ----  | ----  |
| -h \| --help  | Show the help message. |
| -l \| --list-categories  | Lists the tracing categories available to your connected device. |

### Commands and command options
|  Commands and options	   | Description |
|  ----  | ----  |
| -o file  | Write the HTML trace report to the specified file. If you don't specify this option, rheatrace saves your report to the same directory as rheatrace.py and names it systrace.html.|
| -t N \| --time=N  | Trace device activity for N seconds. If you don't specify this option, rheatrace prompts you to end the trace by pressing the Enter key from the command line. |
|  -b N \| --buf-size=N  | Use a trace buffer size of N kilobytes. This option lets you limit the total size of the data collected during a trace.|
|  -k functions\|--ktrace=functions  | Trace the activity of specific kernel functions, specified in a comma-separated list.|
|  -a app-name\|--app=app-name  | Enable tracing for specified app, this option must be set. The app must contain tracing instrumentation calls from the Trace class. You should specify this option whenever you profile your app—many libraries, such as RecyclerView, include tracing instrumentation calls that provide useful information when you enable app-level tracing.|
|  --from-file=file-path  | Create an interactive HTML report from a file, such as TXT files that include raw trace data, instead of running a live trace.|
|  -e device-serial\|--serial=device-serial  | Conduct the trace on a specific connected device, identified by its device serial number.|
| categories	| Include tracing information for the system processes you specify, such as gfx for system processes that render graphics. You can run rheatrace with the -l command to see a list of services available to your connected device.|

### Extensive options

|  Global options   | Description |
|  ----  | ----  |
| -r \| --restart  | Before tracing app activities, force to stop the app and restart it. |
| -v \| --version  | Check the current rheatrace version. |
| --advanced-sys-time \| --advanced-sys-time  | How long in advance to open systrace, default value is 2 seconds.|
| --debug \| --debug | Whether to enable debug log output. |


## Easter Egg Feature

RheaTrace provides Gradle compilation configuration to change the tracking behavior of the App when it is running, but it needs to be repackaged every time, which work low efficiency. So we provide file configuration ways to change tracking behavior.

Create a Properties format file named `rheatrace.config`, the configurations that can be written are:

```
io=true
classLoad=true
memory=true
mainThreadOnly=true
atraceBufferSize=100000
startWhenAppLaunch=true
```

Then, push the file to the device directory `sdcard/rhea-trace/${application package name}`, and then restart the application to take effect.

| options   | defaults  | description  |
|  ----  | ----  | ----  |
| io  | true | Whether to enable tracking io native related methods.|
| classLoad  | fasle | Whether to enable tracking class loading events, only supports Android 8.0 and above, and the app build type is debugable|
| memory  | fasle | Whether to enable tracking memory access events, only supports Android 8.0 and above, and the app build type is debugable.|
| mainThreadOnly  | fasle | Whether to capture trace events only on the main thread, if you only care about the main thread trace events, please set it to true.|
| atraceBufferSize  | 100000 | Specify the size of the ring buffer to store atrace data in the memory. If its value is too small, the trace data will be incompletely written. If you capture multi-threaded trace data, it is recommended to set the value to about one million; the minimum value is 10,000. The maximum value is 5 million.|
| startWhenAppLaunch  | true | Start to capture tracking events at the beginning of the app startup. If you do startup optimization, it is recommended to keep the value as true.|



## Known Issues

1. RheaTrace only support python2.7, please check your python environment.
2. RheaTrace does not currently support Windows.
3. RheaTrace only support capture main process trace events.
4. RheaTrace need read and write permissions of external storage, so you should grant the permission manually.
5. if you can't open output file `systrace.html` directly, please using `perfetto` to load it.

## Support
1. Communicate on GitHub issues.
2. Study the source code.
3. Check [wiki](./INTRODUCTION.MD) or FAQ for help.
4. Contact us kisson_cjw@hotmail.com.
5. Join QQ group chat.

![](./assets/btrace-qq.jpeg)

## Contributing

[Contributing Guide](./CONTRIBUTING.MD)

## License

[Apache License](./LICENSE)
