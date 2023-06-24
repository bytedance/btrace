# btrace 2.0
![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-2.0.0-red.svg?style=flat)

## 重大更新
1. 使用体验：支持 Windows 啦！此外将脚本实现从 Python 切至 Java 并去除各种权限要求，因脚本工具可用性问题引起的用户使用打断次数几乎降为 0，同时还将 Trace 产物切至 PB 协议，产物体积减小 70%，网页打开速度提升 7 倍！
2. 性能体验：通过大规模改造方法 Trace 逻辑，将 App 方法 Trace 底层结构由字符串切换为整数，实现内存占用减少 80%，存储改为 mmap 方式、优化无锁队列逻辑、提供精准插桩策略等，百万量级方法全插桩下性能损耗进一步降低至 15%！
3. 监控数据：新增 4 项数据监控能力，重磅推出渲染详情采集能力！同时还新增 Binder、线程启动、Wait/Notify/Park/Unpark 等详情数据！

## 接入
### 项目配置
在您项目的根目录下build.gradle文件中增加 rhea-gradle-plugin 作为依赖。

``` groovy
buildscript {
    repositories {
        ...
        maven {
            url "https://artifact.bytedance.com/repository/byteX/"
        }
        ...
    }
    dependencies {
        classpath 'com.bytedance.btrace:rhea-gradle-plugin:2.0.0'
    }
}

allprojects {
    repositories {
        ...
        maven {
            url "https://artifact.bytedance.com/repository/byteX/"
        }
        ...
    }
}
```

接着，在app/build.gradle文件中应用如下所示插件和依赖。

``` groovy
dependencies {
    // rheatrace core lib
    implementation "com.bytedance.btrace:rhea-core:2.0.0"
}
...
apply plugin: 'com.bytedance.rhea-trace'
...
rheaTrace {
   compilation {
      traceFilterFilePath = "${project.rootDir}/trace-filter/traceFilter.txt"
      needPackageWithMethodMap = true
      applyMethodMappingFilePath = ""
   }
}
```

### 编译配置
RheaTrace 2.0 简化编译插件参数，仅需要提供 compilation 配置用于控制编译期行为。RheaTrace 1.0 中 runtime 配置已废弃，但为保证兼容升级，我们还是保留 runtime 参数配置，即使配置也不会生效，RheaTrace 2.0 改为通过脚本命令行参数来动态配置，后文将详细介绍。

| 参数 | 默认值  | 说明                                                                                           |
|---|------|----------------------------------------------------------------------------------------------|
| traceFilterFilePath | null | 该文件配置决定哪些方法需要被插桩，详细用法与 RheaTrace 1.0 一致，可以参考 [RheaTrace Gradle 参数介绍](GRADLE_CONFIG.zh-CN.MD)。 |
| applyMethodMappingFilePath | null | 设备方法的 ID，您可以通过指定上次编译输出的 methodMapping.txt 文件路径来保证多次编译的方法 ID 一致。                              |
| needPackageWithMethodMap | true | 是否将 methodMapping.txt 文件打包内置到 apk 内部。                                                        |

## 使用
1. 确保电脑已集成 adb 与 Java 环境。
2. 连接手机至电脑，并保证可被 adb devices 识别。
3. 安装集成 RheaTrace 2.0 的 APK 至手机。
4. 下载下方“脚本管理” 最新脚本至电脑。
5. 在电脑脚本所在目录执行如下命令。

```shell
java -jar rhea-trace-shell.jar -a ${your app package name} -t 10 -o output.pb -r rhea.all sched -fullClassName
```

6.  trace 产物使用 https://ui.perfetto.dev/ 即可打开分析。

### 脚本版本
| 版本号 | 发布日期       | 脚本                             | 更新说明 |
| ---|------------|--------------------------------| ---|
| 2.0.0 | 2023-06-24 | [rhea-trace-shell-2.0.0.jar](https://oss.sonatype.org/service/local/repositories/releases/content/com/bytedance/btrace/rhea-trace-processor/2.0.0/rhea-trace-processor-2.0.0.jar) | 2.0 首次发布 | 

### 参数说明
RheaTrace 2.0 在命令行参数上做了大量改进，更偏于开发者使用。
 
#### 必选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -a $applicationName|N/A|指定您的 App 的包名|

#### 可选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -o $outputPath | 当前时间.pb | 指定产物 trace 保存路径，默认值会根据时间戳自动生成 |
| -t $timeInSecond | 5 |指定这次采集的时长，单位是秒 |
|-mode $mode | 根据设备而定|指定采集 Trace 的模式，目前支持两种模式：<ol><li>perfetto: 8.1 及以上系统默认模式，可以采集到您的应用函数执行 trace 以及系统 atrace 和 ftrace；</li> <li>simple: 8.1 以下系统默认模式，可以采集到您的应用函数执行 trace 以及系统 atrace</li></ol> |
|-maxAppTraceBufferSize $size | 500000000 | 采集 Trace 时的 buffer 大小，单位是字节，一般而言，您不需要配置此参数，除非您遇到下面类似的提醒：<blockquote> MaxAppTraceBufferSize is too small. Expected 100515704 Actual 100000000. Add -maxAppTraceBufferSize 100515704 to your command</blockquote>注意：maxAppTraceBufferSize 仅在 App 启动后的首次 trace 时生效 |
| -threshold $ns | 0 | 采集函数耗时阈值，单位纳秒。采集时长较长时该参数可减小产物体积 | 
| -s $serial | | 指定 adb 连接的设备 |
|  -mainThreadOnly |  | 仅采集主线程 trace |
| -r |  | 自动重启以抓取启动过程的 trace |
| -fullClassName | | trace 信息默认是不包含包名的，此参数可开启包名 |
| rhea.binder || 开启 binder 信息增强 |
| rhea.render || 开启渲染监控能力 |
| rhea.io || 开启 IO 监控能力 |
| rhea.thread || 开启线程创建监控能力 |
| rhea.block || 开启 park/unpark/wait/notify 监控功能 |
| rhea.all || 开启上述所有 RheaTrace 增强的监控能力 |
| -debug || 打印调试日志 |
| --list || 查看设备支持的 category 列表 |

## 技术原理
如果您对 RheaTrace 2.0 实现原理感兴趣，可以查看文档：[btrace 2.0 技术方案简介](INTRODUCTION.MD)。如果有其他疑问，欢迎加入底部的飞书群进行探讨。

## 已知问题

| | 问题 | 建议 | 
| -- | --- | --- | 
| 1 | 部分定制 ROM 可能遇到 Perfetto 采集失败的情况，错误提示如下： <blockquote>Error: systrace file not found:rheatrace.workspace/systemTrace.trace<br/> Tips: your device may not support perfetto. please retry with `-mode simple`. </blockquote> | <ol><li>重启手机后重试；</li><li>如果仍然遇到此问题，可以降级到 -mode simple 重试。</li></ol> |
|2 | 不支持 Perfetto 的设备（主要是8.1以前的系统）上无法采集到 CPU 调度等系统信息。| 建议使用 -mode simple 模式。 | 
|3 | 32 位 apk 容易遇到虚拟内存不足的错误导致崩溃 | <ol><li>换用 64 位包；</li><li>如必须 32 位，建议通过 `-maxAppTraceBufferSize` 调小 bufferSize 减少虚拟内存占用，同时配置 `-threshold` 控制采集方法的阈值。这样可以在较小的虚拟内存空间内采集更长时间的 Trace. </li></ol>| 
|4 | methodMapping 内置到 apk 内部需要将 mergeAssets 依赖于 dexBuilder，如果您的工程也有反向依赖，则会导致循环依赖问题 | 1. 通过配置 needPackageWithMethodMap = false，可以将内置 methodMapping 功能关闭，这样可以解决循环依赖的问题，但是在使用 btrace 功能时需要手动通过 -m 提供 methodMapping 路径。|
|5 | 尚未支持 AGP 7.0+ |  |

## 意见反馈
RheaTrace 2.0 在错误提示上也做了大量的优化，使用中遇到问题建议先通过提示信息尝试解决。当然，也会有一些边界的 case 我们没有做到准确的提示，或者提示的信息不够清晰，欢迎加入下面的`btrace 飞书群`进行反馈，我们将定期处理。也欢迎大家提供其他的反馈与建议！

![](./assets/btrace-lark.png)

## 贡献

[Contributing Guide](./CONTRIBUTING.MD)

## 许可证

[Apache License](./LICENSE)
