// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_transcribe;

import static android.content.Context.AUDIO_SERVICE;
import static android.content.Context.MODE_PRIVATE;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.ViewModel;

import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;

import java.io.File;
import java.util.Arrays;
import java.util.Objects;

public class MainViewModel extends ViewModel {
    Context context;
    Application application;

    Handler mHandler = new Handler(Looper.getMainLooper());

    Thread mainThread;
    boolean mainThreadStopSignal = false;
    int handlerQueueCount = 0;

    boolean mainThreadSleep = false;

    //PowerManager.WakeLock wakeLock;

    DefaultLifecycleObserver lifecycleObserver = new DefaultLifecycleObserver() {
        @Override
        public void onPause(@NonNull LifecycleOwner owner) {
            DefaultLifecycleObserver.super.onPause(owner);
            mainThreadSleep = true;
            nativePauseResumeASR(true);
            Log.i("UHO_Transcribe", "Putting main thread to sleep");
        }

        @Override
        public void onResume(@NonNull LifecycleOwner owner) {
            DefaultLifecycleObserver.super.onResume(owner);
            mainThreadSleep = false;
            nativePauseResumeASR(false);
            Log.i("UHO_Transcribe", "Resuming main thread functionality");
        }
    };

    boolean mainThreadRunning() {
        return (mainThread != null);
    }

    public void start() {
        if(mainThreadRunning())
            return;

        mainThread = new Thread(mainThreadRunnable);
        mainThread.start();
        Log.i("UHO_Transcribe", "Starting main thread inside MainViewModel");
    }

    public MainViewModel() {
        super();
    }

    public void setContext(Context context) {
        this.context = context;
    }
    public void setApplication(Application application) { this.application = application; }
    public void initLifecycle(Lifecycle lifecycle) {
        lifecycle.addObserver(lifecycleObserver);
    }

    @Override
    protected void onCleared() {
        Log.i("UHO_Transcribe", "Stopping the application...");
        // When app is stopped
        super.onCleared();
        // clean up mainThread
        if(mainThread != null) {
            mainThread.interrupt();
            mainThread = null;
        }

        Log.i("UHO_Transcribe", "Done!");
    }

    public interface MainCallback {
        void onReceiveMessage(int msg);
        void onNewText(String text);
    };

    MainCallback mainCallback;

    void setMainCallback(MainCallback callback) {
        mainCallback = callback;
    }

    void sendControlMessage(int m) {
        if(mainCallback != null)
            mainCallback.onReceiveMessage(m);
    }

    void sendPrintoutMessage(String message) {
        if(mainCallback != null)
            mainCallback.onNewText(message);
    }

    Runnable mainThreadRunnable = new Runnable() {
        void cleanup() {
            //mainThread = null;    // (deferred this to code which invokes mainThread.interrupt() in onCleared)
        }

        @Override
        public void run() {
            sendControlMessage(TranscribeActivity.MSG_LOADING);

            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO); //THREAD_PRIORITY_FOREGROUND);

            /*if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
                cleanup();
                sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                return;
            }*/

            if (ActivityCompat.checkSelfPermission(context, android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                cleanup();
                sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                return;
            }

            AssetManager assetManager = ((App)application).getAssetManager();

            Log.i("UHO_Transcribe", String.format("assetManager is %s", (assetManager==null)?"null":"OK"));

            Log.i("UHO_Transcribe", "Starting...");
            AudioRecord audioRecord = null;

            // Record from microphone
            // (changed to UNPROCESSED/MIC on 22.3.2026 as VOICE_RECOGNITION's noise cancellation may cause words not to be picked up by relevant nearby speech sources such as radio or TV);
            // (alternatively you may try MediaRecorder.AudioSource.VOICE_RECOGNITION)
            int audioSource;
            AudioManager audioManager = (AudioManager)context.getSystemService(AUDIO_SERVICE);

            /*if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                String unprocessedProperty = audioManager.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED);
                if ("true".equalsIgnoreCase(unprocessedProperty)) {
                    Log.i("UHO_Transcribe", "[mainThreadRunnable] choosing AudioSource.UNPROCESSED audio source");
                    audioSource = MediaRecorder.AudioSource.UNPROCESSED;
                } else {
                    Log.i("UHO_Transcribe", "[mainThreadRunnable] choosing AudioSource.MIC audio source");
                    audioSource = MediaRecorder.AudioSource.MIC;
                }
            } else {
                audioSource = MediaRecorder.AudioSource.MIC;
            }*/
            audioSource = MediaRecorder.AudioSource.MIC;

            int sampleRate = 16000;
            int channelConfig = AudioFormat.CHANNEL_IN_MONO;
            int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;
            int minBufferSize = 2 * AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
            int targetBufferSize = Math.max(8192 * Float.SIZE / Byte.SIZE, minBufferSize);  // at least half a second of audio

            try {
                audioRecord = new AudioRecord(
                        audioSource,
                        sampleRate,
                        channelConfig,
                        audioFormat,
                        targetBufferSize);
            } catch (Exception e) {
                Log.i("UHO_Transcribe", "Exception message: " + Objects.requireNonNull(e.getMessage()));
                cleanup();
                sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                return;
            }

            Log.i("UHO_Transcribe", "Calling startRecording");
            try {
                audioRecord.startRecording();
            } catch (IllegalStateException e) {
                Log.i("UHO_Transcribe", "Exception message: " + Objects.requireNonNull(e.getMessage()));
                cleanup();
                sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                return;
            }

            // Find out which language we're starting ASR for
            SharedPreferences preferences = context.getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);
            String assetName = preferences.getString("assetName", MainActivity.DEFAULT_CHOSEN_ASSET);
            MainActivity.LanguageInfo chosenLangInfo = MainActivity.langInfo.get(0);
            for(MainActivity.LanguageInfo langInfo: MainActivity.langInfo) {
                if(langInfo.assetName.equals(assetName)) {
                    chosenLangInfo = langInfo;
                    break;
                }
            }

            String fullAsrModelPath = "";
            String fullAsrVocabPath = "";
            String fullPncModelPath = "";
            String fullPncVocabPath = "";

            if(chosenLangInfo.installTime) {
                fullAsrModelPath = chosenLangInfo.asrModelPath;
                fullAsrVocabPath = chosenLangInfo.asrVocabPath;
                if(chosenLangInfo.punctuate) {
                    fullPncModelPath = chosenLangInfo.pncModelPath;
                    fullPncVocabPath = chosenLangInfo.pncVocabPath;
                }
            } else {
                Context applicationContext = context.getApplicationContext();
                AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);
                AssetPackLocation location = assetPackManager.getPackLocation(chosenLangInfo.assetName);
                if(location == null) {
                    cleanup();
                    sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                    return;
                }
                String assetLocation = location.assetsPath();

                {
                    File f = new File(assetLocation, chosenLangInfo.asrModelPath);
                    fullAsrModelPath = f.getAbsolutePath();
                }

                {
                    File f = new File(assetLocation, chosenLangInfo.asrVocabPath);
                    fullAsrVocabPath = f.getAbsolutePath();
                }

                if(chosenLangInfo.punctuate) {
                    File f = new File(assetLocation, chosenLangInfo.pncModelPath);
                    fullPncModelPath = f.getAbsolutePath();
                }

                if(chosenLangInfo.punctuate) {
                    File f = new File(assetLocation, chosenLangInfo.pncVocabPath);
                    fullPncVocabPath = f.getAbsolutePath();
                }
            }

            boolean experimental = preferences.getBoolean("enableExperimental", false);

            // ASR start
            Log.i("UHO_Transcribe", "Calling nativeStartASR:" + "\n" + fullAsrModelPath+ "\n" + fullAsrVocabPath + "\n" + fullPncModelPath + "\n" + fullPncVocabPath);
            nativeStartASR(
                    assetManager,
                    fullAsrModelPath,
                    fullAsrVocabPath,
                    fullPncModelPath,
                    fullPncVocabPath,
                    chosenLangInfo.installTime,
                    chosenLangInfo.isConformer,
                    chosenLangInfo.timeBias,
                    chosenLangInfo.unspacedText,
                    chosenLangInfo.punctuate,
                    chosenLangInfo.pncTokType.ordinal(),
                    chosenLangInfo.pncSymbols,
                    chosenLangInfo.pncUseCapitalLetters,
                    chosenLangInfo.suppressTokens,
                    experimental
                    );

            //sendControlMessage(TranscribeActivity.MSG_STARTED);
            while(true) {
                if(nativeIsReadyASR()) {
                    Log.i("UHO_Transcribe", "Start!");
                    sendControlMessage(TranscribeActivity.MSG_STARTED);
                    break;
                }

                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    sendControlMessage(TranscribeActivity.MSG_STARTERROR);
                    break;
                }
            }

            while (!Thread.interrupted()) {
                if(mainThreadSleep) {
                    try {
                        Thread.sleep(500);
                    } catch (InterruptedException e) {
                        break;
                        //throw new RuntimeException(e);
                    }
                    continue;
                }

                float[] buff = new float[800];
                int numRead = audioRecord.read(buff, 0, buff.length, AudioRecord.READ_BLOCKING);

                float[] addBuff = buff;
                if (numRead < buff.length)
                    addBuff = Arrays.copyOfRange(buff, 0, numRead);

                nativeAddNewAudio(addBuff);

                String nativeFullText = nativeGetNewText();

                //if (!nativeFullText.isEmpty())
                    sendPrintoutMessage(nativeFullText);
            }

            // Cleanup and exit
            audioRecord.stop();
            nativeStopASR();

            cleanup();
            //sendMainActivityBroadcastMessage(TranscribeActivity.MSG_STOPPED);
        }
    };

    public boolean isTablet(Context context) {  // suggested by GPT
        return (context.getResources().getConfiguration().screenLayout
                & Configuration.SCREENLAYOUT_SIZE_MASK)
                >= Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    public native void nativeStartASR(AssetManager assetManager,
                                      String asrModelPath,
                                      String asrVocabPath,
                                      String pncModelPath,
                                      String pncVocabPath,
                                      boolean installTime,
                                      boolean isConformer,
                                      float timeBias,
                                      boolean unspacedText,
                                      boolean punctuate,
                                      int pncType,
                                      char[] pncSymbols,
                                      boolean pncUseCapitalLetters,
                                      boolean suppressTokens,
                                      boolean experimental
                                      );

    public native void nativePauseResumeASR(boolean pause);
    public native void nativeStopASR();

    public native void nativeAddNewAudio(float[] audio);
    public native String nativeGetNewText();
    public native float nativeGetTimeASR();
    public native boolean nativeIsReadyASR();
}
