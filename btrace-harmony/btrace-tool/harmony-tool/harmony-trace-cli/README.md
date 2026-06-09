# harmony-trace-cli

`harmony-trace-cli` 是 Harmony 侧采样 Trace 的命令行工具（命令名：`ohtrace`）。它通过 `hdc` 与设备通信，触发 App 侧开始/停止采样录制，将产物拉取到本地后转换成 Perfetto 可打开的 `pb.gz`，并自动在浏览器中打开 Perfetto UI 查看。

## 功能概览

- 连接设备并选择目标设备（多设备时通过 `-k/--key` 指定）
- 启动/停止采样录制
  - 支持设置 `buffer_size`、`sample_interval`、`main_only`、`high_freq`
  - 支持 `--restart`：重启应用并下发采样配置后再开始录制
- 拉取原始 trace（zip 形式）并在本地生成：
  - 原始 zip：`trace_<timestamp>`
  - Perfetto 文件：`trace_<timestamp>.pb.gz`
- 符号与映射还原
  - ArkTS/JS：支持 `source_mapping`（sourcemap 映射）与 `name_cache`（混淆还原）
  - Native：支持指定 `native_path`（so 目录），通过 `addr2line` 反查符号与内联栈
- 自动启动本地 HTTP 服务并打开 Perfetto UI：
  - 浏览器打开 `https://ui.perfetto.dev/#!/?url=http://127.0.0.1:9001/trace`

## 工作流程

1. 使用 `hdc list targets` 发现并选择设备
2. 通过 `hdc fport` 将设备端口转发到本机端口（端口由 `bundleName` 计算得到）
3. 通过本机 `http://localhost:<port>/record/start` / `/record/stop` 控制 App 侧录制
4. 停止录制后，App 侧返回 base64 编码的多个文件内容，CLI 组装成 zip（`trace_<timestamp>`）
5. 解码 zip 中的 `trace-*.bin`、`mapping.bin` 等，转换为统一 `SamplingFile`，再编码为 Perfetto `pb` 并 gzip
6. （可选）结合 `source_mapping` / `name_cache` / `native_path` 做符号还原
7. 启动单文件 HTTP server 暴露 `pb.gz` 并打开浏览器

## 环境依赖

- Java 11
- `hdc` 在 PATH 中可执行
- `addr2line` 在 PATH 中可执行（用于 native 符号回溯）
  - 通常来自 DevEco Studio / OpenHarmony native LLVM 工具链目录
- 设备侧目标 App 已安装且包含对应的录制/导出能力（由 CLI 调用 `/record/start`、`/record/stop` 触发）

## 构建

在仓库根目录执行：

```bash
./gradlew :harmony-tool:harmony-trace-cli:jar
```

产物位于：

```text
harmony-tool/harmony-trace-cli/build/libs/*.jar
```

## 使用

基本用法：

```bash
java -jar harmony-tool/harmony-trace-cli/build/libs/*.jar -b <bundle_name>
```

常见示例：

```bash
# 录制 30s，输出到指定目录（会在目录下按 bundleName 建子目录）
java -jar build/libs/*.jar -b com.example.app -t 30 -o ~/Desktop/ohtrace

# 多设备时指定目标设备
java -jar build/libs/*.jar -b com.example.app -k <device_key>

# 主线程采样、并启用高频模式
java -jar build/libs/*.jar -b com.example.app -m -H -i 10

# 结合 ArkTS/JS sourcemap 与 nameCache 做符号还原
java -jar build/libs/*.jar -b com.example.app -s /path/to/sourceMaps.map -n /path/to/nameCache.json

# 结合 native so 目录做 native 符号反解
java -jar build/libs/*.jar -b com.example.app -N /path/to/native/so/dir

# 重启应用并下发采样配置后再录制
java -jar build/libs/*.jar -b com.example.app --restart
```

## 参数说明

`ohtrace`（`picocli`）支持的主要参数如下（完整参数以 `--help` 输出为准）：

- `-b, --buldle_name`：App bundle name（必填）
- `-o, --output_path`：输出目录（默认 `~/Desktop/ohtrace`）
- `-t, --time_limit`：录制时长（秒，默认 60）
- `-S, --buffer_size`：App 侧 trace buffer 上限（不传时按时长/采样间隔估算）
- `-i, --sample_interval`：采样间隔（默认 1；高频模式下最小会被提升）
- `-m, --main_only`：仅主线程
- `-H, --high_freq`：高频采样模式（会改变 buffer 估算单位与最小采样间隔）
- `-w, --wait_timeout`：停止录制后等待导出超时（秒，默认 60）
- `-k, --key`：指定设备 key（多设备时需要）
- `-r, --restart`：重启目标应用并下发采样配置后开始录制
- `-s, --source_mapping`：sourcemap 映射文件路径
- `-n, --name_cache`：nameCache 文件路径
- `-N, --native_path`：native so 目录路径

## 输出

- 原始 trace zip：`<output_path>/<bundleName>/trace_<timestamp>`
- Perfetto 文件：`<output_path>/<bundleName>/trace_<timestamp>.pb.gz`
- 执行结束时会自动打开浏览器并加载 Perfetto UI 查看

