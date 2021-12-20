## Gradle 构建参数说明

|  参数   | 默认值  | 说明  |
|  ----  | ----  | ----  |
| **compilation**  | NA | 与编译相关的配置。 |
| &nbsp;traceWithMethodID  | fasle | 为 App 中需要跟踪的方法设置 id 以此来跟踪此自定义事件，好处包括减少 APK 包体积增量以及性能提升。 |
| &nbsp;traceFilterFilePath  | null | 该文件配置决定哪些方法您不希望跟踪，详细用法见下文。|
| &nbsp;applyMethodMappingFilePath  | null | 用特指定方法 ID 来设置自定义事件名称，即您可以指定方法的 ID 值。|
| **runtime**  | NA | 与 App 运行相关的配置。 |
| &nbsp;mainThreadOnly  | fasle | 仅在主线程抓取跟踪事件，如果您仅关心主线程 trace 数据，请将其置为 true。|
| &nbsp;startWhenAppLaunch  | true | 在 App 启动之初开始抓取跟踪事件，如果您做启动优化，建议将值保持为 true。 |
| &nbsp;atraceBufferSize  | "100000" | 指定内存存储 atrace 数据 ring buffer 的大小，如果其值过小会导致 trace 数据写入不完整，若您抓取多线程 trace 数据，建议将值设为百万左右量级；最小值为 1 万，最大值为 5 百万。|

### traceFilterFilePath 配置规则

目前 RheaTrace 支持如下。

```
-allowpackage rhea/sample/android
-blockpackage rhea/sample/android/test

-allowclassmethodswithparametervalues rhea.sample.android.app.RheaApplication {
   printApplicationName(*java.lang.String);
}

-blockclassmethods rhea.sample.android.app.MainActivity {
   onCreate(android.os.Bundle);
}
```
`-allowpackage`指定哪些包名下类的方法插桩。

`-blockpackage`指定哪些包名下类的方法**不**插桩。

当`-allowpackage`配置后，`-blockpackage`不能随意设置，`-blockpackage`包名设置必须以`-allowpackage`设置的包名开头，且必须是其子包。

如上述例子，`-allowpackage`设置了‘rhea/sample/android’，那么`-blockpackage`只能设置 ‘rhea/sample/android’ 下的子包 'rhea/sample/android/test'。


若您需要知道 App 运行过程中某些方法参数的值，那么`-allowclassmethodswithparametervalues`将助您一臂之力。如上示例，配置类名、方法名、方法签名，在您关注方法参数类型描述前加上`*`，若关注多个参数值，分别加上`*`即可。效果如下，中括号`[rhea.sample.android.app.RheaApplication]`中数据即表示参数的值。

```
 .sample.android-23670 [001] ...1 1388264.149291: tracing_mark_write: B|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
 .sample.android-23670 [001] ...1 1388264.149391: tracing_mark_write: E|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
```
RheaTrace 提供方法粒度的过滤规则`-blockclassmethods`，您可以配置指定方法不做插桩处理，比如一些高频函数或一定不耗时方法等。

### applyMethodMappingFilePath 配置规则

当 `traceWithMethodID` 值为 true，会在 app/build/outputs 目录下生成一个 `methodMapping` 文件，格式如下。

```
1,17,rhea.sample.android.app.FirstFragment$onViewCreated$1 onClick (Landroid/view/View;)V
2,17,rhea.sample.android.app.SecondFragment$onViewCreated$1 onClick (Landroid/view/View;)V
3,4,rhea.sample.android.app.RheaApplication attachBaseContext (Landroid/content/Context;)V
4,1,rhea.sample.android.app.FirstFragment onCreateView (Landroid/view/LayoutInflater;Landroid/view/ViewGroup;Landroid/os/Bundle;)Landroid/view/View;
5,1,rhea.sample.android.app.MainActivity onCreateOptionsMenu (Landroid/view/Menu;)Z
6,1,rhea.sample.android.app.RheaApplication onCreate ()V
7,1,rhea.sample.android.app.FirstFragment onViewCreated (Landroid/view/View;Landroid/os/Bundle;)V
8,1,rhea.sample.android.app.MainActivity onOptionsItemSelected (Landroid/view/MenuItem;)Z
9,1,rhea.sample.android.app.MainActivity onWindowFocusChanged (Z)V
10,17,rhea.sample.android.app.MainActivity$onCreate$1 onClick (Landroid/view/View;)V
11,8,rhea.sample.android.app.MainActivity$onCreate$1 <clinit> ()V
12,1,rhea.sample.android.app.SecondFragment onCreateView (Landroid/view/LayoutInflater;Landroid/view/ViewGroup;Landroid/os/Bundle;)Landroid/view/View;
13,1,rhea.sample.android.app.SecondFragment onViewCreated (Landroid/view/View;Landroid/os/Bundle;)V
1048574,1,android.os.Handler dispatchMessage (Landroid.os.Message;)V
```
其格式为：**方法 ID, 方法 accessFlag, 类名, 方法名, 方法签名, 方法返回值**，您按照如上格式配置需要固定 ID 的方法即可。

