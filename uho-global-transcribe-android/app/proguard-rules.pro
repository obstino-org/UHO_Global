# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# Uncomment this to preserve the line number information for
# debugging stack traces.
#-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
#-renamesourcefileattribute SourceFile

-dontoptimize
-keep class android.webkit.** { *; }

# consider -keep class your.package.** { *; } ?

# Kotlin
-keep class kotlin.Metadata { *; }
-keepattributes *Annotation*,Signature,InnerClasses,EnclosingMethod,RuntimeVisibleAnnotations,RuntimeVisibleParameterAnnotations

# ViewModel
-keep class com.obstino.uho_transcribe.MainViewModel { *; }

# WorkManager
-keep class com.obstino.uho_transcribe.DownloaderWorkerFactory { *; }
-keep class com.obstino.uho_transcribe.** extends androidx.work.ListenableWorker { *; }
-keep class com.obstino.uho_transcribe.** extends androidx.work.Worker { *; }
-keep class com.obstino.uho_transcribe.** extends androidx.work.CoroutineWorker { *; }

# WebView JS bridge, only if used
-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}
