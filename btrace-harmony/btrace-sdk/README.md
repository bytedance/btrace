# btrace for HarmonyOS

![](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)

[README 中文版](./README.zh-CN.md)

Record trace data for HarmonyOS apps to help find performance issues.

## Integration

Add the on-device SDK to your HarmonyOS project via ohpm:

```bash
ohpm install @bytedance/btrace
```

Initialize the offline server as early as possible during app startup:

```typescript
import { OfflineServer } from '@bytedance/btrace'

OfflineServer.Init()
```

## Command-Line Tool Installation

### Prerequisites

- JDK 11 or above is required, with environment variables configured properly.
- The `hdc` tool is required to operate apps on the device. Add it to your `PATH`:

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains:$PATH"
```

- Parsing native symbols requires `llvm-addr2line`. Add it to your `PATH`:

```bash
export PATH="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin:$PATH"
```

### Install

1. Download the latest `harmony-trace-cli` jar and place it under `~/.oh_trace_cli` directory.

> Note: The download artifacts will be published with the official release. Please stay tuned.

## Usage

Run the following command in the directory where `harmony-trace-cli.jar` is located:

```bash
java -jar harmony-trace-cli.jar -b ${your_bundle_name} -t 10
```

The above command starts tracing for the given bundle from its current foreground page, samples all threads for 10 seconds, saves the result to the current directory and automatically generates a flame graph.

### Stop

```bash
ctrl + c
```

Stops the tracing and automatically exports the trace file from the app sandbox.

### Parameters Description

#### Required Parameters

| Parameter | Default Value | Description |
|---|---|---|
| -b, --bundle_name $bundleName | N/A | Specifies the bundle name of your app. |

#### Optional Parameters

| Parameter | Default Value | Description |
|---|---|---|
| -h, --help | | Show help. |
| -k, --key $deviceKey | | Specifies the device connected by hdc. <ul><li>If only one device is connected, it will be chosen automatically.</li><li>If multiple devices are connected, prompt the user to make a selection.</li></ul> |
| -o, --output_path $outputPath | ~/Desktop/ohtrace | Specifies the path where the trace artifact is saved. |
| -t, --time_limit $timeLimit | 60 | Specifies the maximum tracing duration in seconds. Tracing will stop automatically when the duration is reached. |
| -N, --native_path $nativePath | | Absolute path to the **directory** containing native `.so` files for symbolication. |
| -s, --source_mapping $sourceMapping | | Absolute path to the sourceMap file. |
| -n, --name_cache $nameCache | | Absolute path to the NameCache file. |
| -i, --sample_interval $sampleInterval | | Sampling interval. <ul><li>In normal mode: unit is ms, minimum is 1ms.</li><li>In high-frequency mode: unit is us, minimum is 50us.</li></ul> |
| -H, --high_freq | | Enable high-frequency sampling mode. |
| -S, --buffer_size $bufferSize | | Specifies how many call stack samples can be stored. |
| -m, --main_only | | Only collect call stacks of the main thread. |
| -r, --restart | | Launch / relaunch the app and start tracing from the launch stage. |
| -sp, --skip_perfetto | | Skip generating the flame graph via Perfetto after the trace is exported. |
| -a, --all_symbol | | Display all symbols including system symbols (system symbols are hidden by default). |

### Known Issues

- **CLI is currently macOS-only**: the `harmony-trace-cli` command-line tool only supports macOS at the moment; Windows / Linux support will be added in future releases.
- **On-device symbol resolution can be slow**: the SDK currently performs symbol resolution on-device, which is relatively time-consuming. After the trace capture finishes, exporting from the device and writing to disk may take noticeably longer; we will continue to optimize this.
- **System library symbols must be supplied manually**: the SDK cannot currently resolve symbols of HarmonyOS system libraries on-device. To recover the symbols, users need to supply the directory containing the matching `.so` files via `-N, --native_path`, and the CLI tool will perform symbolication offline.

## License

[Apache License](../../LICENSE)
