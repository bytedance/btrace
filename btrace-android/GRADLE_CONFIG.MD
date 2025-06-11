## Gradle Build Parameter Description

| Parameter              | Default Value | Description |
| -------------------------- | ----------------- | ----------------|
| traceFilterFilePath        | null              | This file configuration determines which methods you don't want to trace. See detailed usage below.|
| applyMethodMappingFilePath | null              | Set custom event names using a specific method ID, i.e., you can specify the ID value of the method.|
| needPackageWithMethodMap   | true      | whether the methodMapping will be packaged into the APK during compilation. |

### traceFilterFilePath Configuration Rules

Currently, RheaTrace supports the following:

```
# Specify which package name clases' methods to instrument.
-allowpackage rhea/sample/android

# Specify which package name clases' methods to instrument.
-allowpackage rhea/sample/android

# Instrument methods related to locks.
-tracesynchronize

# Instrument call points of Native methods.
-tracenative

# Instrument Aidl methods.
-traceaidl

# Instrument methods containing loops.
-traceloop

# Disable instrumentation of time-consuming method calls by default.
-disabledefaultpreciseinstrumentation

# Enable large method instrumentation, methods with a utility count exceeding 40.
-tracelargemethod 40

# The caller of this method needs to be instrumented.
-traceclassmethods rhea.sample.android.app.PreciseInstrumentationTest {
   test
}

# Methods annotated with this annotation need to be instrumented.
-tracemethodannotation org.greenrobot.eventbus.Subscribe

# All methods of this Class will be instrumented.
-traceclass io.reactivex.internal.observers.LambdaObserver

# The parameter information of this method will be retained in Trace.
-allowclassmethodswithparametervalues rhea.sample.android.app.RheaApplication {
   printApplicationName(*java.lang.String);
}
```

When `-allowpackage` is configured, `-blockpackage` cannot be set arbitrarily. The package name configured in `-blockpackage` must start with the package name set in `-allowpackage` and must be its subpackage.

For example, in the above case, since `-allowpackage` is set to 'rhea/sample/android', `-blockpackage` can only be set to the subpackage 'rhea/sample/android/test' under 'rhea/sample/android'.

If you need to know the values of certain method parameters during the App's operation, `-allowclassmethodswithparametervalues` can help. As shown in the example above, configure the class name, method name, and method signature, and add an asterisk `*` before the parameter type description you are concerned with. If you are concerned with multiple parameter values, add an asterisk `*` to each. The effect is as follows: the data in the square brackets [rhea.sample.android.app.RheaApplication] represents the value of the parameter.

```
 .sample.android-23670 [001] ...1 1388264.149291: tracing_mark_write: B|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
 .sample.android-23670 [001] ...1 1388264.149391: tracing_mark_write: E|23670|RheaApplication:printApplicationName[rhea.sample.android.app.RheaApplication]
```

### applyMethodMappingFilePath Configuration Rules

When the `traceWithMethodID` value is set to `true`, a `methodMapping` file will be generated in the `app/build/outputs` directory. The format is as follows:

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

The format is: `Method ID`, `Method accessFlag`, `Class Name`, `Method Name`, `Method Signature`, `Method Return Value`. You can configure the methods with a fixed ID according to the above format.