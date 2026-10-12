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
# --- 1. Accessibility Service ---
# The Android OS instantiates AccessibilityServices via reflection from the Manifest.
-keep public class * extends android.accessibilityservice.AccessibilityService

# --- 2. WorkManager Workers ---
-keep class com.obstino.uho_captions.DownloaderWorkerFactory { *; }
-keep class com.obstino.uho_captions.** extends androidx.work.ListenableWorker { *; }
-keep class com.obstino.uho_captions.** extends androidx.work.Worker { *; }
-keep class com.obstino.uho_captions.** extends androidx.work.CoroutineWorker { *; }

# --- 3. ANRWatchdog ---
# Preserves ANRWatchdog background threads and error callbacks from being stripped.
-keep class com.github.anrwatchdog.** { *; }

# --- 4. LocalBroadcastManager ---
# LocalBroadcastManager is part of legacy support libraries; if using standard local broadcasts,
# standard rules apply, but ensure your custom BroadcastReceivers (if any) are kept:
-keep class * extends android.content.BroadcastReceiver {
    public <init>();
}