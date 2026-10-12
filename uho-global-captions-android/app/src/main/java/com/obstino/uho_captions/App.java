// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_captions;

import android.app.Activity;
import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.work.Configuration;
import androidx.work.Operation;
import androidx.work.WorkManager;

import com.github.anrwatchdog.ANRWatchDog;

public class App extends Application {
    public static final String CHANNEL_ID = "uhoGlobalServiceChannel";
    private static boolean appInForeground = false;
    //public static boolean flagCancelUniqueWork = true;

    private AssetManager globalAssetManager;
    static public boolean downloaderJustFinished = false;

    // Used to load the 'uho_captions' library on application startup.
    static {
        System.loadLibrary("uho");
    }

    @Override
    public void onCreate() {
        super.onCreate();

        // reset the flag that is normally used to signalize to MainActivity when large download requires confirmation
        SharedPreferences prefs = getApplicationContext().getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);

        {
            SharedPreferences.Editor ed = prefs.edit();
            ed.putBoolean("assetConfirmationRequired", false);
            ed.apply();
        }

        globalAssetManager = getApplicationContext().getResources().getAssets();
        new ANRWatchDog().start();

        //flagCancelUniqueWork = true;

        // provide custom configuration
        Configuration myConfig = new Configuration.Builder()
                .setMinimumLoggingLevel(android.util.Log.INFO)
                .setWorkerFactory(new DownloaderWorkerFactory())
                .build();

        WorkManager.initialize(this, myConfig);

        // COMMENTED CODE BELOW (because no need to cancel previously enqueued download)
        // on app start, cancel previous download workers (so they won't auto restart)
        //WorkManager workManager = WorkManager.getInstance(getApplicationContext());
        //    workManager.cancelUniqueWork(MainActivity.uniqueWorkName);

        createNotificationChannel();

        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle savedInstanceState) {}
            @Override
            public void onActivityDestroyed(@NonNull Activity activity) {}
            @Override
            public void onActivityPaused(@NonNull Activity activity) {
                appInForeground = false;
            }
            @Override
            public void onActivityResumed(@NonNull Activity activity) {
                appInForeground = true;
            }
            @Override
            public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle outState) {}
            @Override
            public void onActivityStarted(@NonNull Activity activity) {}
            @Override
            public void onActivityStopped(@NonNull Activity activity) {}
        });
        //new ANRWatchDog().start();
    }

    public static boolean isAppInForeground() {
        return appInForeground;
    }

    // Made with help of https://www.youtube.com/watch?v=FbpD5RZtbCc
    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {   // do if check eventhough we're always >= Oreo
            NotificationChannel serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "UHO Global Service Channel",
                    NotificationManager.IMPORTANCE_HIGH
            );

            NotificationManager manager = getSystemService(NotificationManager.class);
            manager.createNotificationChannel(serviceChannel);
        }
    }

    public AssetManager getAssetManager() {
        return globalAssetManager;
    }
}
