// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_transcribe;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Handler;

import androidx.work.Configuration;
import androidx.work.WorkManager;

import com.github.anrwatchdog.ANRWatchDog;


public class App extends Application {
    //public static final String CHANNEL_ID = "uhoServiceChannel";
    //static Handler handler = new Handler();

    private AssetManager globalAssetManager;
    static public boolean downloaderJustFinished = false;

    // Used to load the 'uho' library on application startup.
    static {
        System.loadLibrary("uho");
    }

    @Override
    public void onCreate() {
        super.onCreate();

        globalAssetManager = getApplicationContext().getResources().getAssets();

        new ANRWatchDog().start();

        // provide custom configuration
        Configuration myConfig = new Configuration.Builder()
                .setMinimumLoggingLevel(android.util.Log.INFO)
                .setWorkerFactory(new DownloaderWorkerFactory())
                .build();
        //initialize WorkManager
        WorkManager.initialize(this, myConfig);
        //createNotificationChannel();
    }

    public AssetManager getAssetManager() {
        return globalAssetManager;
    }

    /*
    // Made with help of https://www.youtube.com/watch?v=FbpD5RZtbCc
    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {   // do if check eventhough we're always >= Oreo
            NotificationChannel serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "UHO Service Channel",
                    NotificationManager.IMPORTANCE_DEFAULT
            );

            NotificationManager manager = getSystemService(NotificationManager.class);
            manager.createNotificationChannel(serviceChannel);
        }
    }*/
}
