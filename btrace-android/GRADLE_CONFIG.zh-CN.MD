## Gradle 构建参数说明

| **参数**                     | **默认值**   | **说明**                                                                                                     |
| -------------------------- | --------- | ---------------------------------------------------------------------------------------------------------- |
| traceFilterFilePath        | null      | 该文件配置决定哪些方法您不希望跟踪，详细用法见下文。                                                                                 |
| applyMethodMappingFilePath | null      | 用特指定方法 ID 来设置自定义事件名称，即您可以指定方法的 ID 值。                                                                       |
| needPackageWithMethodMap   | true      | 默认编译时，将产物methodMap一起打进apk                                                                                  |

### traceFilterFilePath 配置规则

目前 RheaTrace 支持如下。

```
# 指定哪些包名下类的方法插桩。

-allowpackage rhea/sample/android

# 指定哪些包名下类的方法不插桩。

-blockpackage rhea/sample/android/test



# 对锁相关的方法插桩

-tracesynchronize



# 对Native方法的调用点插桩

-tracenative



# 对Aidl方法插桩

-traceaidl



# 对包含循环的方法插桩

-traceloop



# 关闭默认耗时方法的调用插桩

-disabledefaultpreciseinstrumentation



# 开启大方法插桩，方法效用数超过40

-tracelargemethod 40



# 该方法的调用方需要进行插桩

-traceclassmethods rhea.sample.android.app.PreciseInstrumentationTest {

   test

}



# 被该注解修饰的方法需要被插桩

-tracemethodannotation org.greenrobot.eventbus.Subscribe



# 该Class的所有方法均会被插桩

-traceclass io.reactivex.internal.observers.LambdaObserver



# 该方法的参数信息会在Trace中保留

-allowclassmethodswithparametervalues rhea.sample.android.app.RheaApplication {

   printApplicationName(*java.lang.String);

}
```

当`-allowpackage`配置后，`-blockpackage`不能随意设置，`-blockpackage`包名设置必须以`-allowpackage`设置的包名开头，且必须是其子包。

如上述例子，`-allowpackage`设置了‘rhea/sample/android’，那么`-blockpackage`只能设置 ‘rhea/sample/android’ 下的子包 'rhea/sample/android/test'。

若您需要知道 App 运行过程中某些方法参数的值，那么`-allowclassmethodswithparametervalues`将助您一臂之力。如上示例，配置类名、方法名、方法签名，在您关注方法参数类型描述前加上`*`，若关注多个参数值，分别加上`*`即可。效果如下，中括号`[rhea.sample.android.app.RheaApplication]`中数据即表示参数的值。

```
 .sample.android-23670 [001] ...1 1388264.149291: tracing_mark_write: B|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]

 .sample.android-23670 [001] ...1 1388264.149391: tracing_mark_write: E|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
```

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