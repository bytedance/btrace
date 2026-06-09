# 更新日志

本文件记录 **@bytedance/btrace**（btrace for HarmonyOS）的全部重要变更。

## [1.0.0] - 2026-06-05

### 新增

- btrace for HarmonyOS 首次正式发布，基于 btrace 3.0 同步采样抓栈方案构建。
- 借助 `HiDebug_Backtrace_Object` 实现 ArkTS / Native 一体化栈回溯，单一 API 同时拿到 ArkTS 帧与 Native 帧。
- 同步抓栈 + 异步信号兜底双路径：在动态桩点处同步抓栈，对长循环、阻塞调用等"采样空窗"通过实时信号补样。
- 可中断系统调用代理（`SlowSysCallProxy`）：对 `epoll_wait` / `recv` / `nanosleep` / `poll` 等阻塞调用透明 hook，进入前屏蔽采样信号，退出后恢复，并在 enter / leave 边界主动抓栈，彻底规避 `EINTR` 副作用。
- 两级 Ring Buffer 流水线 + `CallstackTable` 共享栈节点的存储架构，兼顾高并发写入与低内存占用。
- 耗时归因数据：线程级 CPU Time、Native 内存分配（`malloc / mmap` 等）、字节级内存与字符串操作、文件 I/O。
- 输出 perfetto 兼容的 `.pb` 文件，可直接在 [ui.perfetto.dev](https://ui.perfetto.dev) 打开分析。
- 配套命令行工具 `harmony-trace-cli`，用于触发采集、导出 trace 文件并自动生成火焰图。

### 已知问题

- 命令行工具暂时仅支持 macOS，Windows / Linux 适配将在后续版本提供。
- 端内符号解析整体耗时较高，trace 采集结束后从设备导出与落盘的等待时间会比较明显，后续会持续优化。
- 当前 SDK 在端内无法解析鸿蒙系统库的符号；如需还原，用户需通过 `-N, --native_path` 参数手动指定包含对应 `.so` 的目录，由命令行工具在离线阶段完成符号化。
