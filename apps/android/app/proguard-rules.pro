# JNI: native methods are looked up by name.
-keepclasseswithmembernames class * { native <methods>; }
-keep class org.embermesh.app.core.EmberNative { *; }
# The WebView bridge is called by name from JavaScript.
-keepclassmembers class org.embermesh.app.bridge.EmberBridge { @android.webkit.JavascriptInterface <methods>; }
-dontwarn com.google.zxing.**
