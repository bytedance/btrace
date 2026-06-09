# btrace 3.0
![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-3.0.0-red.svg?style=flat)

[English](./README.md)

[btrace for Android](#btrace-for-android)

[btrace for iOS](#btrace-for-ios)

[btrace for HarmonyOS](#btrace-for-harmonyos)

## 重磅更新

此次更新，我们重磅推出 btrace 3.0 版本，提出了业界首创的同步抓栈的 Trace 采集方案，实现了高性能的 Trace 采集。此外，新版本在支持 Android 系统的基础上，新增了对 iOS 系统的全面支持。同时，我们将 btrace 扩展到了 HarmonyOS 平台，提供端上 SDK 与命令行工具，帮助开发者采集 HarmonyOS 应用的 Trace 数据并快速定位性能问题。

## btrace for Android

### 接入

在项目 app/build.gradle 文件中添加如下依赖：

``` groovy
dependencies {
    if (enable_btrace == 'true') {
        implementation 'com.bytedance.btrace:rhea-inhouse:3.0.0'
    } else {
        implementation 'com.bytedance.btrace:rhea-inhouse-noop:3.0.0'
    }
}
```

在项目根目录 gradle.properties 文件中增加 enable_btrace 开关：

```
# 需要打 Trace 包的时候把这个开关值改为 true
enable_btrace=false
```

在应用 Application 的 `attachBaseContext()` 方法中增加如下初始化逻辑：

``` java

public class MyApp extends Application {

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        // 当 enable_btrace=false 时集成的是 rhea-inhouse-noop，此时的 init() 方法里没有任何逻辑
        RheaTrace3.init(base);
    }
}

```

### 使用

1. 确保电脑已集成 adb 与 Java 环境。
2. 连接手机至电脑，并保证可被 adb devices 识别。
3. 安装集成 btrace 3.0 的 APK 至手机。
4. 下载下方“脚本版本”中的最新脚本至电脑。
5. 在电脑脚本所在目录执行如下命令。

```shell
java -jar rhea-trace-shell.jar -a ${your_package_name} -t 10 -o output.pb -r sched
```

6. trace 产物使用 https://ui.perfetto.dev/ 即可打开分析。

#### 脚本版本
| 版本号 | 发布日期       | 脚本                             | 更新说明 |
| ---|------------|--------------------------------| ---|
| 3.0.0 | 2025-06-3 | [rhea-trace-shell-3.0.0.jar](https://oss.sonatype.org/service/local/repositories/releases/content/com/bytedance/btrace/rhea-trace-processor/3.0.0/rhea-trace-processor-3.0.0.jar) | 3.0 首次发布 | 

#### 参数说明


##### 必选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -a $applicationName|N/A|指定您的 App 的包名|

##### 可选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -o $outputPath | ${applicationName}_yyyy_MM_dd_HH_mm_ss.pb | 指定产物 trace 保存路径，默认值会根据 app 包名和当前时间戳自动生成 |
| -t $timeInSecond | 5 |指定这次采集的时长，单位是秒。<br>注意：在 MacOS 上不指定采集时间会进入交互式采集模式，此时通过按下回车键结束采集；当前 Windows 不支持交互式采集模式，必须指定采集时长。 |
| -m $mappingPath | | 如果是 Release 的混淆包，需要指定解混淆的 mapping 文件<br>注意：这里不再是 btrace 2.0 时期的 methodMapping 文件，是 proguard 生成的 mapping 文件。
| -mode $mode | 根据设备而定|指定采集 Trace 的模式，目前支持两种模式：<ol><li>perfetto: 8.1 及以上系统默认模式，可以采集到您的应用函数执行 trace 以及系统 atrace 和 ftrace；</li> <li>simple: 8.1 以下系统默认模式，可以采集到您的应用函数执行 trace 以及系统 atrace</li></ol> |
|-maxAppTraceBufferSize $size | 200000 | 采集 Trace 时的 buffer 最多保存的抓栈数量，单位：次，如果抓栈数据过多就得抓栈数据会被覆盖。 |
| -sampleInterval $ns | 1000000 | 采集时的最小抓栈间隔限制，单位：纳秒 |
| -waitTraceTimeout | 20 | 等待 trace 写入完成并拉取到 PC 端的超时等待时间，单位：秒 |
| -s $serial | | 指定 adb 连接的设备 |
| -mainThreadOnly |  | 仅采集主线程 trace |
| -r |  | 自动重启以抓取启动过程的 trace |
| --list || 查看设备支持的 category 列表 |


### 已知问题及说明

| | 问题 | 建议 | 
| -- | --- | --- | 
| 1 | 当前仅支持 Android 8.0 及以上的设备 | 使用 Android 8.0 及以上的设备采集 Trace |
| 2 | Java 对象创建监控暂未适配 Android 15 及以上设备 | 需要查看对象内存分配信息或需要更详细 Trace 的情况请使用 Android 15 以下版本的设备。 |
| 3 | 不支持 Perfetto 的设备上无法采集到 CPU 调度等系统信息。| 建议使用 -mode simple 模式。 | 
| 4 | 32 位设备或应用采集不到 Trace | 请在 64 位设备上安装并使用 64 位应用。| 

## btrace for iOS

不依赖 Instruments 的，适用于本地环境的 iOS Trace 采集工具，用于帮助发现 app 中潜在的性能问题。

### Installation

Clone 代码，并将 btrace 添加到 Podfile 中:

```ruby
pod 'BTrace', :subspecs => ['Core', 'Debug'], :path => 'xxx/btrace-iOS'
pod 'fishhook', :git => 'https://github.com/facebook/fishhook.git', :branch => 'main'
```

安装命令行工具：

```bash
# 使用 homebrew
brew install libusbmuxd
brew install poetry

# 在 BTraceTool 目录下执行
poetry install
```

### Usage

执行命令前先激活虚拟环境。
```bash
# 在 BTraceTool 目录下执行
poetry shell
# 或者
poetry env activate
```

#### Record

如果未指定'-l'参数，在调用采集命令前 app 必须处于前台运行状态。

```bash
python3 -m btrace record [-h] [-i DEVICE_ID] [-b BUNDLE_ID] [-o OUTPUT] [-t TIME_LIMIT] [-d DSYM_PATH] [-m] [-l] [-s]
```
##### Options
| 选项 | 描述          |
|---------------------------------------| -----------  |
| -h, --help                | 显示帮助      |
| -i DEVICE_ID, --device_id DEVICE_ID |  设备id. 未指定时: <br>· 如果 Mac 只连接了一台设备, 会自动选择该设备 <br>· 如果 Mac 连接了多台设备, 提示用户做出选择 |
| -b BUNDLE_ID    --bundle_id BUNDLE_ID | app 的 bundle id |
| -o OUTPUT       --output OUTPUT | 产物输出路径. 未指定时, 数据会被保存在'~/Desktop/btrace'目录下|
| -t TIME_LIMIT   --time_limit TIME_LIMIT | 最长采集时间, 默认值 3600s |
| -d DSYM_PATH    --dsym_path DSYM_PATH |  符号表路径, 在 Xcode Debug 时可以使用构建出的 app 的路径. 如果指定，会在采集结束后自动绘制火焰图 |
| -m    --main_thread_only |  如果指定，就会只采集主线程数据 |
| -l    --launch  |  如果指定, 会自动启动/重启 app，并开始采集数据|
| -s    --sys_symbol  |  如果指定, 会符号化系统库中的方法 |
##### Examples
```bash
python3 -m btrace record -i xxx -b xxx -d /xxxDebug-iphoneos/xxx.app
python3 -m btrace record -i xxx -b xxx -d /xxxDebug-iphoneos/xxx.dSYM
```

#### Stop
```bash
ctrl + c
```

#### Parse

什么时候需要使用 parse 命令?
- 执行 'record' 命令时未指定 '-d' 选项.
- 重新打开解析过的数据

```bash
python3 -m btrace parse [-h] [-d DSYM_PATH] [-f] [-s] file_path
```
##### Options
| Option                       | Description          |
|---------------------------------------| -----------  |
| -h, --help                | 显示帮助     |
| -d DSYM_PATH, --dsym_path DSYM_PATH |  符号表路径, 在 Xcode Debug 时可以使用构建出的 app 的路径 |
| -s, --sys_symbol  |  如果指定, 会符号化系统库中的方法 |
| -f, --force  |  如果指定, 会强制重新解析数据 |
##### Examples
```bash
btrace parse -d /xxx.dSYM xxx.sqlite
btrace parse -d /xxx.app xxx.sqlite
```

## btrace for HarmonyOS

适用于 HarmonyOS 应用的 Trace 采集工具，用于帮助发现 app 中潜在的性能问题。

### 接入

通过 ohpm 在 HarmonyOS 工程中安装端上 SDK：

```bash
ohpm install @bytedance/btrace
```

在 app 启动尽可能早的位置完成初始化：

```typescript
import { OfflineServer } from '@bytedance/btrace'

OfflineServer.Init()
```

### 命令行工具安装

#### 前置依赖

- 需要 JDK 11 及以上版本，并配置好相应环境变量。
- 需要使用 `hdc` 工具操作 app，因此需要添加如下环境变量：

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains:$PATH"
```

- 解析 native 符号需要使用 `llvm-addr2line`，因此需要添加如下环境变量：

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin:$PATH"
```

#### 安装步骤

1. 下载最新版 `harmony-trace-cli` jar 包，放置到 `~/.harmony-trace-cli` 目录下。

### 使用

在 `harmony-trace-cli.jar` 所在目录下执行如下命令：

```bash
java -jar harmony-trace-cli.jar -b ${your_bundle_name} -t 10
```

上述命令会针对指定包名的 app，从其当前已打开的页面开始采集所有线程的调用栈，采集时长 10 秒，结果保存到当前目录，并自动产出火焰图。

#### 停止采集

```bash
ctrl + c
```

停止采集并自动从沙盒导出 trace 文件。

#### 参数说明

##### 必选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -b, --bundle_name $bundleName | N/A | 指定您的 App 的包名。 |

##### 可选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -h, --help | | 显示帮助信息。 |
| -k, --key $deviceKey | | 指定 hdc 连接的设备。<ul><li>电脑只连接了一台设备时，自动选择该设备；</li><li>电脑连接了多台设备时，提示用户进行选择。</li></ul> |
| -o, --output_path $outputPath | ~/Desktop/ohtrace | 指定 trace 产物的输出路径。 |
| -t, --time_limit $timeLimit | 60 | 指定最大采集时长，单位：秒。超过该时间后自动停止采集。 |
| -N, --native_path $nativePath | | 包含 so 的**文件夹**绝对路径，用于 native 符号解析。 |
| -s, --source_mapping $sourceMapping | | sourceMap 文件的绝对路径。 |
| -n, --name_cache $nameCache | | NameCache 文件的绝对路径。 |
| -i, --sample_interval $sampleInterval | | 采样间隔。<ul><li>普通模式下，单位 ms，最小值 1ms；</li><li>高采样率模式下，单位 us，最小值 50us。</li></ul> |
| -H, --high_freq | | 开启高采样率模式。 |
| -S, --buffer_size $bufferSize | | 指定保存的调用栈样本数量。 |
| -m, --main_only | | 仅采集主线程调用栈。 |
| -r, --restart | | 启动/重启 app 并从启动阶段开始采集数据。 |
| -sp, --skip_perfetto | | 导出 trace 之后跳过使用 Perfetto 生成火焰图。 |
| -a, --all_symbol | | 展示包括系统符号在内的所有符号（默认不展示系统符号）。 |

#### 已知问题

- **命令行工具暂时仅支持 macOS**：`harmony-trace-cli` 当前只在 macOS 上验证可用，Windows / Linux 适配将在后续版本提供；
- **端内符号解析耗时较长**：当前 SDK 在端内进行符号解析时整体耗时较高，trace 采集结束后从设备导出与落盘的等待时间会比较明显，后续会持续优化；
- **系统库符号需用户提供**：当前 SDK 在端内无法解析出鸿蒙系统库的符号；如需还原符号，用户需通过 `-N, --native_path` 参数手动指定包含对应 `.so` 的目录，由命令行工具在离线阶段完成符号化。

## 技术原理
如果您对 btrace 3.0 实现原理感兴趣，可以查看文档：[btrace 3.0 方案原理介绍](INTRODUCTION.zh-CN.MD)。如果有其他疑问，欢迎加入底部的飞书群进行探讨。

## 意见反馈

如在使用 btrace 过程中遇到问题，欢迎加入下面的`btrace 飞书群`进行反馈，我们将定期处理。也欢迎大家提供其他的反馈与建议！

![](./assets/b/zh/lark.png)

## 贡献

[Contributing Guide](./CONTRIBUTING.MD)

## 许可证

[Apache License](./LICENSE)