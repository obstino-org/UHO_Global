package com.obstino.uho_captions;

import static android.content.Context.MODE_PRIVATE;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.work.Data;
import androidx.work.ForegroundInfo;
import androidx.work.Worker;
import androidx.work.WorkerParameters;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;
import com.google.android.play.core.assetpacks.AssetPackState;
import com.google.android.play.core.assetpacks.AssetPackStateUpdateListener;
import com.google.android.play.core.assetpacks.AssetPackStates;
import com.google.android.play.core.assetpacks.model.AssetPackStatus;
import com.google.common.util.concurrent.ListenableFuture;

import java.util.Collections;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

import com.google.common.util.concurrent.Futures;

public class Downloader extends Worker {
    private static final String TAG = Downloader.class.getSimpleName();
    public static final String PROGRESS = "PROGRESS";

    String assetPackName;
    AtomicBoolean result = new AtomicBoolean(false);
    CountDownLatch latch = new CountDownLatch(1);

    boolean lastWaitingForWifi = false;

    public Downloader(
            @NonNull Context appContext,
            @NonNull WorkerParameters workerParams) {
        super(appContext, workerParams);
        this.assetPackName = getInputData().getString("packname");
        Log.i("UHO_Captions", "this.assetPackName = " + this.assetPackName);
    }

    void sendMainActivityBroadcastMessage(int m) {
        Intent intent = new Intent(MainActivity.BROADCAST_EVENT_NAME);
        intent.putExtra("msg", m);
        //intent.setPackage(getPackageName());
        LocalBroadcastManager.getInstance(getApplicationContext()).sendBroadcast(intent);
    }

    AssetPackStateUpdateListener stateUpdateListener = new AssetPackStateUpdateListener() {
        @Override
        public void onStateUpdate(@NonNull AssetPackState assetPackState) {
            Context applicationContext = getApplicationContext();
            AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);

            int status = assetPackState.status();

            switch(status) {
                case AssetPackStatus.REQUIRES_USER_CONFIRMATION:
                case AssetPackStatus.WAITING_FOR_WIFI:
                    Log.i("UHO_Captions", "[onStateUpdate] status = " + ((status == AssetPackStatus.WAITING_FOR_WIFI)?"waiting for wifi":"requires user confirmation"));
                    if(!lastWaitingForWifi) {
                        SharedPreferences prefs = getApplicationContext().getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);
                        // (both set shared pref flag AND send broadcast, as the latter may not be received
                        //  during app being in background (onPause state))
                        SharedPreferences.Editor ed = prefs.edit();
                        ed.putBoolean("assetConfirmationRequired", true);
                        ed.apply();
                        sendMainActivityBroadcastMessage(MainActivity.MSG_CELLULAR_DIALOG);
                    }
                    break;
                case AssetPackStatus.COMPLETED:
                    result.set(true);
                    latch.countDown();
                    break;
                case AssetPackStatus.DOWNLOADING:
                    long bytesDownloaded = assetPackState.bytesDownloaded();
                    long totalBytesToDownload = assetPackState.totalBytesToDownload();

                    int percentDone;
                    if(totalBytesToDownload > 0)
                        percentDone = (int) ((double) bytesDownloaded / (double) totalBytesToDownload * 100.0);
                    else
                        percentDone = 0;

                    setProgressAsync(new Data.Builder().putInt(PROGRESS, percentDone).build());
                    break;
                case AssetPackStatus.CANCELED:
                case AssetPackStatus.FAILED:
                    result.set(false);
                    latch.countDown();
                    break;
                default:
                    Log.i("UHO_Captions", "[onStateUpdate] status = " + status);
            }

            lastWaitingForWifi = (status == AssetPackStatus.REQUIRES_USER_CONFIRMATION ||
                                  status == AssetPackStatus.WAITING_FOR_WIFI);
        }
    };

    private ForegroundInfo createForegroundInfo() {
        int NOTIFICATION_ID = 101;
        String CHANNEL_ID = "UHO_Captions_CHANNEL";
        Context context = getApplicationContext();

        // Create Notification Channel (Required for Android 8.0+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, "Download", NotificationManager.IMPORTANCE_LOW);
            NotificationManager manager = context.getSystemService(NotificationManager.class);
            if (manager != null) manager.createNotificationChannel(channel);
        }

        Notification notification = new NotificationCompat.Builder(context, CHANNEL_ID)
                .setContentTitle("Downloading File")
                .setSmallIcon(R.drawable.ikona)
                .setOngoing(true)
                .build();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return new ForegroundInfo(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } else {
            return new ForegroundInfo(NOTIFICATION_ID, notification);
        }
    }

    @NonNull
    @Override
    public ListenableFuture<ForegroundInfo> getForegroundInfoAsync() {
        return Futures.immediateFuture(createForegroundInfo());
    }

    void cleanup(boolean cancelAsset) {
        Context applicationContext = getApplicationContext();
        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);
        assetPackManager.unregisterListener(stateUpdateListener);
        if(cancelAsset) {
            assetPackManager.cancel(Collections.singletonList(assetPackName));
        }
    }

    @NonNull
    @Override
    public Result doWork() {
        App.downloaderJustFinished = false;   // set to false initially

        Context applicationContext = getApplicationContext();
        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);

        // reset the flag that is normally used to signalize to MainActivity when large download requires confirmation
        SharedPreferences prefs = getApplicationContext().getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);
        SharedPreferences.Editor ed = prefs.edit();
        ed.putBoolean("assetConfirmationRequired", false);
        ed.apply();

        // if its already installed, return success
        {
            AssetPackLocation packLocation = assetPackManager.getPackLocation(assetPackName);
            if (packLocation != null) {
                App.downloaderJustFinished = true;
                return Result.success();
            }
        }

        // fetch asset pack
        assetPackManager.registerListener(stateUpdateListener);
        if(isStopped()) {
            cleanup(true);
            App.downloaderJustFinished = true;
            return Result.failure();
        }

        assetPackManager.fetch(Collections.singletonList(assetPackName))
                .addOnFailureListener(new OnFailureListener() {
                    @Override
                    public void onFailure(@NonNull Exception e) {
                        latch.countDown();
                    }
                });

        try {
            latch.await();
        } catch (InterruptedException e) {
            //throw new RuntimeException(e);
        }

        if(result.get()) {
            //ed.putBoolean("DownloadJustFinished", true);
            //ed.apply();
            cleanup(false);
            App.downloaderJustFinished = true;
            return Result.success();
        } else {
            cleanup(true);
            App.downloaderJustFinished = true;
            return Result.failure();
        }
    }

    @Override
    public void onStopped() {
        super.onStopped();
        cleanup(true);
        latch.countDown();
    }
};
