# btrace for HarmonyOS

![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)

[English](./README.md)

适用于 HarmonyOS 应用的 Trace 采集工具，用于帮助发现 app 中潜在的性能问题。

## 接入

通过 ohpm 在 HarmonyOS 工程中安装端上 SDK：

```bash
ohpm install @bytedance/btrace
```

在 app 启动尽可能早的位置完成初始化：

```typescript
import { OfflineServer } from '@bytedance/btrace'

OfflineServer.Init()
```

## 命令行工具安装

### 前置依赖

- 需要 JDK 11 及以上版本，并配置好相应环境变量。
- 需要使用 `hdc` 工具操作 app，因此需要添加如下环境变量：

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains:$PATH"
```

- 解析 native 符号需要使用 `llvm-addr2line`，因此需要添加如下环境变量：

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin:$PATH"
```

### 安装步骤

1. 下载最新版 `harmony-trace-cli` jar 包，放置到 `~/.harmony-trace-cli` 目录下。

## 使用

在 `harmony-trace-cli.jar` 所在目录下执行如下命令：

```bash
java -jar harmony-trace-cli.jar -b ${your_bundle_name} -t 10
```

上述命令会针对指定包名的 app，从其当前已打开的页面开始采集所有线程的调用栈，采集时长 10 秒，结果保存到当前目录，并自动产出火焰图。

### 停止采集

```bash
ctrl + c
```

停止采集并自动从沙盒导出 trace 文件。

### 参数说明

#### 必选参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| -b, --bundle_name $bundleName | N/A | 指定您的 App 的包名。 |

#### 可选参数

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

### 已知问题

- **命令行工具暂时仅支持 macOS**：`harmony-trace-cli` 当前只在 macOS 上验证可用，Windows / Linux 适配将在后续版本提供；
- **端内符号解析耗时较长**：当前 SDK 在端内进行符号解析时整体耗时较高，trace 采集结束后从设备导出与落盘的等待时间会比较明显，后续会持续优化；
- **系统库符号需用户提供**：当前 SDK 在端内无法解析出鸿蒙系统库的符号；如需还原符号，用户需通过 `-N, --native_path` 参数手动指定包含对应 `.so` 的目录，由命令行工具在离线阶段完成符号化。

## 许可证

[Apache License](../../LICENSE)
