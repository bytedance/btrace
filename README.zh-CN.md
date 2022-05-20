## btrace

[README English version](./README.MD)

![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-1.0.1-red.svg?style=flat)

btrace(又名 RheaTrace) 是一个基于 [Systrace](https://developer.android.com/topic/performance/tracing) 实现的高性能 Android trace 工具，它支持在 App 编译期间自动注入自定义事件，并使用 [bhook](https://github.com/bytedance/bhook) 额外提供 IO 等 native 事件。


## 关键特征

支持自动注入自定义事件，在编译 Apk 期间为 App 方法自动注入**Trace#beginSection(String)** 和 **Trace#endSection()**。

提供额外 IO 等 native 事件，方便定位耗时原因。

支持仅采集主线程 trace 事件。

使用便捷，稳定性高，性能优于 Systrace。

## 开始

在您项目根目录下 build.gradle 文件中增加 rhea-gradle-plugin 作为依赖。

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

接着在 app/build.gradle 文件中应用如下所示插件和依赖。

```
dependencies {
    //rheatrace core lib
    implementation "com.bytedance.btrace:rhea-core:1.0.1"
}
...
rheaTrace {

   compilation {
      //为减少 APK 体积, 你可以为 App 中需要跟踪的方法设置 id 以此来跟踪此自定义事件, 默认值 false。
      traceWithMethodID = false 
      //该文件配置决定哪些方法您不希望跟踪, 默认值 null。
      traceFilterFilePath = "${project.rootDir}/rhea-trace/traceFilter.txt"
      //用特指定方法 id 来设置自定义事件名称, 默认值 null。
      applyMethodMappingFilePath = "${project.rootDir}/rhea-trace/keep-method-id.txt"
  }

   runtime {
      //仅在主线程抓取跟踪事件, 默认值 false。
      mainThreadOnly true 
      //在 App 启动之初开始抓取跟踪事件, 默认值 true。
      startWhenAppLaunch true
      //指定内存存储 atrace 数据 ring buffer 的大小。
      atraceBufferSize "500000"
   }
}
...
apply plugin: 'com.bytedance.rhea-trace'
```

关于 `rheaTrace `，从[RheaTrace Gradle Config](./GRADLE_CONFIG.zh-CN.MD)中了解更多信息。

最后，检测您电脑 python 版本，由于 Systrace 的关系 RheaTrace 仅支持 python 2.7 版本，请将 systrace 环境变量配置在 ***~/.bash_profile*** 文件中。

```
export PATH=${PATH}:/Users/${user_name}/Library/Android/sdk/platform-tools/systrace
```

## 使用

打开终端，将目录切换至 **btrace/scripts/python/rheatrace** 下，并执行如下命令。

```
python rheatrace.py -v
```
接着您将在终端看到如下输出。

```
Current version is 1.0.1.
```

RheaTrace 保留 [Systrace command](https://developer.android.com/topic/performance/tracing/command-line) 命令参数，如果您之前使用过 Systrace，您将很快了解如何使用 RheaTrace。

### 语法

如需为应用生成 HTML 报告，您需要使用以下语法通过命令行运行 `rheatrace`：

```
python rheatrace.py [options] [categories]
```

例如，以下命令会调用 `rheatrace` 来记录设备活动，并生成一个名为 mynewtrace.html 的 HTML 报告。此类别列表是大多数设备的合理默认列表。

```
python rheatrace.py -a rhea.sample.android -t 5 -o ./output/mynewtrace.html sched freq idle am wm gfx view binder_driver hal dalvik camera input res
```
### 全局选项
|  全局选项	   | 说明 |
|  ----  | ----  |
| -h \| --help  | 显示帮助消息。|
| -l \| --list-categories  | 列出您的已连接设备可用的跟踪类别。|

### 命令和命令选项
|  命令和选项	  | 说明 |
|  ----  | ----  |
| -o file  | 将 HTML 跟踪报告写入指定的文件。如果您未指定此选项，rheatrace 会将报告保存到 rheatrace.py 所在的目录中，并将其命名为 systrace.html。|
| -t N \| --time=N  | 跟踪设备活动 N 秒。如果您未指定此选项，systrace 会提示您在命令行中按 Enter 键结束跟踪。|
|  -b N \| --buf-size=N  | 使用 N KB 的跟踪缓冲区大小。使用此选项，您可以限制跟踪期间收集到的数据的总大小。|
|  -k functions\|--ktrace=functions  | 跟踪逗号分隔列表中指定的特定内核函数的活动。|
|  -a app-name\|--app=app-name  | 指定 app 开启 trace 功能, 该选项必须设置。该应用必须包含 Trace 类中的跟踪检测调用。您应在分析应用时指定此选项。很多库（例如 RecyclerView）都包括跟踪检测调用，这些调用可在您启用应用级跟踪时提供有用的信息。如需了解详情，请参阅定义自定义事件。|
|  --from-file=file-path  | 根据文件（例如包含原始跟踪数据的 TXT 文件）创建交互式 HTML 报告，而不是运行实时跟踪。|
|  -e device-serial\|--serial=device-serial  | 在已连接的特定设备（由对应的设备序列号标识）上进行跟踪。|
| categories	| 包含您指定的系统进程的跟踪信息，如 gfx 表示用于渲染图形的系统进程。您可以使用 -l 命令运行 systrace，以查看已连接设备可用的服务列表。|
| -d N \| --deeplink=N | 通过deeplink链接启动APP。|

### 拓展选项

|  全局选项   | 说明 |
|  ----  | ----  |
| -r \| --restart  | 在跟踪 App 活动前, 强制关闭 App 并重启。|
| -v \| --version  | 查看当前 rheatrace 版本。|
| --advanced-sys-time \| --advanced-sys-time  | 提前多长时间开启 systrace，默认值是 2 秒。|
| --debug \| --debug | 是否开启 debug 日志输出。|

## 彩蛋功能

RheaTrace 提供 Gradle 编译配置改变 App 运行时的 tracing 行为，但该方式每次需要重新打包，效率很低。为此，我们提供文件配置方式来改变 tracing 行为。

创建名字为 `rheatrace.config` 的 Properties 格式文件，可写入的配置有：

```
io=true
classLoad=true
memory=true
mainThreadOnly=true
atraceBufferSize=100000
startWhenAppLaunch=true
```
接着，将该文件 push 至设备目录 `sdcard/rhea-trace/${应用包名}` 下，然后重启应用即可生效。

|  参数   | 默认值  | 说明  |
|  ----  | ----  | ----  |
| io  | true | 是否开启跟踪 io native 相关方法。 |
| classLoad  | fasle | 是否开启跟踪类加载事件，仅支持 Android 8.0 及以上，且 App 编译类型为 debuggable。 |
| memory  | fasle | 是否开启跟踪内存访问事件，仅支持 Android 8.0 及以上，且 App 编译类型为 debuggable。|
| mainThreadOnly  | fasle | 是否仅在主线程抓取跟踪事件，如果您仅关心主线程 trace 数据，请将其置为 true。|
| atraceBufferSize  | 100000 | 指定内存存储 atrace 数据 ring buffer 的大小，如果其值过小会导致 trace 数据写入不完整，若您抓取多线程 trace 数据，建议将值设为百万左右量级；最小值为 1 万，最大值为 5 百万。|
| startWhenAppLaunch  | true | 在 App 启动之初开始抓取跟踪事件，如果您做启动优化，建议将值保持为 true。|


## 已知问题

1. RheaTrace 仅支持 python2.7，请注意检查 python 环境。
2. RheaTrace 暂不支持 Windows。
3. RheaTrace 仅支持采集主进程的 trace 事件。
4. RheaTrace 需要外置存储的读写权限，因此您需要手动赋予该权限。
5. 如果您无法直接打开输出产物 `systrace.html` ，请用 `perfetto` 加载。

## 支持

1. 在 Github issue 上交流。
2. 阅读源码。
3. 查阅 [wiki](./INTRODUCTION.MD) 或 FAQ 寻求帮助。
4. 联系我 kisson_cjw@hotmail.com。
5. 加入 QQ 群。

![](./assets/btrace-qq.jpeg)

## 贡献

[Contributing Guide](./CONTRIBUTING.MD)

## 许可证

[Apache License](./LICENSE)


















