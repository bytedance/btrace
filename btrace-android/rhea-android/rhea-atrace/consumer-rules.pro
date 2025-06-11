# keep @CriticalNative on method
-keep @interface dalvik.annotation.optimization.CriticalNative
-keepclassmembers class * {
    @dalvik.annotation.optimization.CriticalNative *;
}
-dontwarn com.bytedance.rheatrace.atrace.BinaryTrace