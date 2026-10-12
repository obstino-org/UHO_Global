package com.obstino.uho_transcribe;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.work.ListenableWorker;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;

public class DownloaderWorkerFactory extends WorkerFactory {
    DownloaderWorkerFactory() {
    }

    @Nullable
    @Override
    public ListenableWorker createWorker(
            @NonNull Context appContext,
            @NonNull String workerClassName,
            @NonNull WorkerParameters workerParameters) {

        if(workerClassName.equals(DownloaderWorkerFactory.class.getName())) {
            return new Downloader(appContext, workerParameters);
        }

        return null;
    }
}
