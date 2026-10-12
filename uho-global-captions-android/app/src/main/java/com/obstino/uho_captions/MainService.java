// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_captions;

import static android.app.Activity.RESULT_CANCELED;
import static android.app.Activity.RESULT_OK;
import static com.obstino.uho_captions.App.CHANNEL_ID;

import android.Manifest;
import android.accessibilityservice.AccessibilityService;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.Display;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.widget.RemoteViews;
import android.widget.TextView;
import android.graphics.Typeface;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;

import java.io.File;
import java.util.Arrays;
import java.util.Objects;
import java.util.PriorityQueue;
import java.util.Queue;
import java.util.concurrent.CountDownLatch;

public class MainService extends AccessibilityService {
    static MainService serviceInstance = null;

    float layoutDefaultAlpha = 0.8f;   // almost fully visible

    Handler mHandler = new Handler(Looper.getMainLooper());

    static final String BROADCAST_EVENT_NAME = "MainServiceBroadcast";

    Thread mainThread;
    boolean mainThreadStopSignal = false;
    int handlerQueueCount = 0;

    View layout;    // overlay layout
    WindowManager.LayoutParams layoutParams;
    final Object layoutLock = new Object();

    TextTouchListener textViewTouchListener;

    MediaProjection mediaProjection;

    SharedPreferences prefs;

    //PowerManager.WakeLock wakeLock;

    //@Nullable
    //@Override
    //public IBinder onBind(Intent intent) {
    //    return null;
    //}

    boolean mainThreadRunning() {
        return (mainThread != null);
    }

    MainService.MyBroadcastReceiver myBroadcastReceiver = new MainService.MyBroadcastReceiver();

    class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int msg = intent.getIntExtra("msg", -1);
            Log.i("UHO_Captions", "[MainService.MyBroadcastReceiver] msg = " + msg);
            parseMessage(msg, context, intent);
        }

        public void parseMessage(int msg, Context context, Intent intent) {
            switch(msg) {
                case MainActivity.MSG_CAN_INITSTART:
                    // This executes upon Start button press
                    // -- if handlerQueueCount > 0, start is already in running, return "can't init start"
                    // -- if handlerQueueCount = 0 and main thread is running, we can safely signalize to stop
                    // -- otherwise, if main thread isn't running, activity can show permission dialogs and start
                    switch(getSpeechRecognitionState()) {
                        case waitstart:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_CANT_INITSTART);
                            break;
                        case started:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_CAN_INITSTOP);
                            break;
                        case stopped:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_CAN_INITSTART);
                            break;
                    }

                    break;

                case MainActivity.MSG_GETSTARTSTATE:
                    switch(getSpeechRecognitionState()) {
                        case waitstart:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_WAITSTART);
                            break;
                        case started:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_STARTED);
                            break;
                        case stopped:
                            sendMainActivityBroadcastMessage(MainActivity.MSG_STOPPED);
                            break;
                    }
                    break;

                case MainActivity.MSG_STARTSTOP:
                    Log.i("UHO_Captions", "Service inside MSG_STARTSTOP");

                    onNewTextReset();

                    SpeechRecognitionState recognitionState = getSpeechRecognitionState();

                    if(recognitionState == SpeechRecognitionState.waitstart) {
                        Log.i("UHO_Captions", "Service sending MSG_STOPPED");
                        sendMainActivityBroadcastMessage(MainActivity.MSG_STOPPED);
                        break;
                    }

                    if(recognitionState == SpeechRecognitionState.started) {
                        Log.i("UHO_Captions", "[MainService:MyBroadcastReceiver:parseMessage] setting mainThreadStopSignal to true");
                        mainThreadStopSignal = true;    // TODO race condition check
                        break;
                    }

                    Log.i("UHO_Captions", "Service 22222");
                    int resultCode = intent.getIntExtra("code", RESULT_CANCELED);

                    // if ASR is not active and invalid resultCode, return error
                    if (resultCode != RESULT_OK) {
                        Log.i("UHO_Captions", "bad resultCode");
                        sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                        break;
                    }

                    // ASR is not active, use intent extras to obtain the mediaProjection token
                    Log.i("UHO_Captions", "Getting mediaProjection inside MainService!");

                    // startForeground with media projection type can be called only *after* user approved MP permission
                    // (otherwise we get an exception)
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                        startForeground(1, buildNotification(false), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
                    else
                        startForeground(1, buildNotification(false));

                    handlerQueueCount++;
                    mHandler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            // getMediaProjection can be called only after startForeground AND after user allowed MP
                            // (otherwise we get an exception)
                            Intent data = intent.getParcelableExtra("data");
                            MediaProjectionManager mediaProjectionManager = (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
                            mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, data);

                            mainThread = new Thread(mainThreadRunnable);
                            mainThread.start();
                            handlerQueueCount--;
                            Log.i("UHO_Captions", "Starting main thread inside MainService");
                        }
                    }, 1000);
                    break;
                case MainActivity.MSG_FORCESTOP:
                    Log.i("UHO_Captions", "Calling disableSelf from MainService upon user request");

                    synchronized (layoutLock) {
                        if (layout != null) {
                            WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
                            TextView textView = layout.findViewById(R.id.textview_transcription);
                            textView.setOnTouchListener(null);
                            windowManager.removeView(layout);
                            layout = null;
                        }
                    }
                    disableSelf();
                    break;
            }
        }
    }

    // Triggered when we start the service (called every time startService is called, can be done multiple times -- Intent contains info)
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if(intent == null)
            return START_NOT_STICKY;

        if(intent.getAction() != null && "ACTION_STARTSTOP".equals(intent.getAction())) {
            Log.i("UHO_Captions", "ACTION_STARTSTOP");

            if(!isSpeechRecognitionActive()) {
                Log.i("UHO_Captions", "ASR is *not* active. Launching ACTION_SHOW_AND_START in MainActivity");
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        Intent mainActivityIntent = new Intent(getApplicationContext(), MainActivity.class);
                        mainActivityIntent.setAction("ACTION_SHOW_AND_START");
                        // these flags assure that onNewIntent will always be executed in MainActivity
                        mainActivityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        mainActivityIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
                        mainActivityIntent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
                        getApplicationContext().startActivity(mainActivityIntent);

                        //Intent broadcastIntent = new Intent(MainService.BROADCAST_EVENT_NAME);
                        //broadcastIntent.putExtra("msg", MainActivity.MSG_STARTSTOP);
                        //sendBroadcast(broadcastIntent);
                    }
                }, 500);
            } else {
                Log.i("UHO_Captions", "[MainService:onStartCommand] ASR *is* active, sending MSG_STARTSTOP to service");
                Intent broadcastIntent = new Intent(MainService.BROADCAST_EVENT_NAME);
                broadcastIntent.putExtra("msg", MainActivity.MSG_STARTSTOP);
                LocalBroadcastManager.getInstance((Context)serviceInstance).sendBroadcast(broadcastIntent);
            }
        }

        /*
        if(textViewTouchListener == null)
            textViewTouchListener = new TextTouchListener(this);

        int msg = intent.getIntExtra("msg", -1);
        switch(msg) {
            case MainActivity.MSG_CAN_INITSTART:
                // This executes upon Start button press
                // -- if handlerQueueCount > 0, start is already in running, return "can't init start"
                // -- if handlerQueueCount = 0 and main thread is running, we can safely signalize to stop
                // -- otherwise, if main thread isn't running, activity can show permission dialogs and start
                if(handlerQueueCount > 0) {
                    sendMainActivityBroadcastMessage(MainActivity.MSG_CANT_INITSTART);
                } else if(mainThreadRunning()) {
                    sendMainActivityBroadcastMessage(MainActivity.MSG_CAN_INITSTOP);
                }
                else {
                    sendMainActivityBroadcastMessage(MainActivity.MSG_CAN_INITSTART);
                }
                break;

            //case MainActivity.MSG_CAN_OPENSETTINGS:
            //    if(mainThreadRunning() || handlerQueueCount > 0)
            //        sendMainActivityBroadcastMessage(MainActivity.MSG_CANT_OPENSETTINGS);
            //    else
            //        sendMainActivityBroadcastMessage(MainActivity.MSG_CAN_OPENSETTINGS);
            //
            //    break;

            case MainActivity.MSG_GETSTARTSTATE:
                if(handlerQueueCount > 0)
                    sendMainActivityBroadcastMessage(MainActivity.MSG_WAITSTART);
                else if(mainThreadRunning())
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STARTED);
                else
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STOPPED);

                break;

            case MainActivity.MSG_STARTSTOP:
                Log.i("UHO_Captions", "Service inside MSG_STARTSTOP");
                if(handlerQueueCount > 0) {
                    // signalize that it's currently still in stopped state
                    Log.i("UHO_Captions", "Service sending MSG_STOPPED");
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STOPPED);
                    break;
                }

                Log.i("UHO_Captions", "Service 11111");
                if(mainThreadRunning()) {
                    mainThreadStopSignal = true;
                    break;
                }

                Log.i("UHO_Captions", "Service 22222");
                int resultCode = intent.getIntExtra("code", RESULT_CANCELED);

                // if ASR is not active and invalid resultCode, return error
                if(resultCode != RESULT_OK) {
                    Log.i("UHO_Captions", "bad resultCode");
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                    break;
                }

                // ASR is not active, use intent extras to obtain the mediaProjection token
                Log.i("UHO_Captions", "Getting mediaProjection inside MainService!");

                // startForeground with media projection type can be called only *after* user approved MP permission
                // (otherwise we get an exception)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                    startForeground(1, buildNotification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
                else
                    startForeground(1, buildNotification());

                handlerQueueCount++;
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        // getMediaProjection can be called only after startForeground AND after user allowed MP
                        // (otherwise we get an exception)
                        Intent data = intent.getParcelableExtra("data");
                        MediaProjectionManager mediaProjectionManager = (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
                        mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, data);

                        mainThread = new Thread(mainThreadRunnable);
                        mainThread.start();
                        handlerQueueCount--;
                        Log.i("UHO_Captions", "Starting main thread inside MainService");
                    }
                }, 1000);
                break;
        }*/

        return START_STICKY;
    }

    Notification buildNotification(boolean currentlyStopped) {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_MUTABLE);

        Intent startStopIntent;
        PendingIntent pendingIntentStartStop;
        String buttonText;

        if(!currentlyStopped) {
            buttonText = "STOP";
            startStopIntent = new Intent(this, MainService.class);
            startStopIntent.setAction("ACTION_STARTSTOP");
            pendingIntentStartStop = PendingIntent.getService(this, 0, startStopIntent, PendingIntent.FLAG_MUTABLE);
        } else {    // (currentlyStopped = false)
            buttonText = "START";
            // Instructs MainActivity to show mediaprojection permissions dialog
            // ** note: if notification panel is right in front of MainActivity, activity will be brought to focus, but may not receive ACTION.
            //   (TODO handle this edge case)
            Log.i("UHO_Captions", "[MainService:buildNotification] else statement");
            startStopIntent = new Intent(this, MainActivity.class);
            startStopIntent.setAction("ACTION_SHOW_AND_START");
            startStopIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            pendingIntentStartStop = PendingIntent.getActivity(this, 0, startStopIntent, PendingIntent.FLAG_MUTABLE);
        }

        RemoteViews remoteView = new RemoteViews(getPackageName(), R.layout.notification_layout);
        remoteView.setTextViewText(R.id.button_notification, buttonText);
        remoteView.setOnClickPendingIntent(R.id.button_notification, pendingIntentStartStop);

        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentInfo("UHO service")
                //.setContentText("UHO")
                .setSmallIcon(R.drawable.ic_stat_name)
                .setContentIntent(pendingIntent)
                //.addAction(buttonAction)
                .setStyle(new NotificationCompat.DecoratedCustomViewStyle())
                .setCustomContentView(remoteView)
                .setCustomBigContentView(remoteView)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setSilent(true)
                .build();

        return notification;
    }

    // Called the first time we start the service
    @Override
    public void onCreate() {
        serviceInstance = this;

        //PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        //PowerManager.WakeLock wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "MyApp::MyWakelockTag");
        //wakeLock.acquire();

        prefs = getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);

        if(textViewTouchListener == null)
            textViewTouchListener = new TextTouchListener(this);

        super.onCreate();
    }

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();

        LocalBroadcastManager.getInstance(getApplicationContext()).registerReceiver(myBroadcastReceiver,
                new IntentFilter(MainService.BROADCAST_EVENT_NAME));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i("UHO_Captions", "[MainService.onDestroy]");
        LocalBroadcastManager.getInstance(getApplicationContext())
                .unregisterReceiver(myBroadcastReceiver);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.i("UHO_Captions", "[MainService.onUnbind]");
        /*mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                System.exit(0);
            }
        }, 1000);*/

        return super.onUnbind(intent);
    }

    // When service is stopped
    /*
    @Override
    public void onDestroy() {
        if(wakeLock != null)
            wakeLock.release();

        // clean up mainThread
        while(mainThread != null) {
            mainThreadStopSignal = true;

            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        super.onDestroy();
    }*/

    enum SpeechRecognitionState {
        waitstart,
        started,
        stopped
    }

    SpeechRecognitionState getSpeechRecognitionState() {
        if (handlerQueueCount > 0)
            return SpeechRecognitionState.waitstart;
        else if (mainThreadRunning())
            return SpeechRecognitionState.started;
        else
            return SpeechRecognitionState.stopped;
    }

    boolean isSpeechRecognitionActive() {
        // it's 100% in MainActivity.MSG_STOPPED state
        if (handlerQueueCount > 0)
            return true;    // infact it's in MainActivity.MSG_WAITSTART state (start-in-progress state)
        else return mainThreadRunning();    // it's 100% in MainActivity.MSG_STARTED state
    }

    void sendMainActivityBroadcastMessage(int m) {
        Intent intent = new Intent(MainActivity.BROADCAST_EVENT_NAME);
        intent.putExtra("msg", m);
        //intent.setPackage(getPackageName());
        LocalBroadcastManager.getInstance((Context)serviceInstance).sendBroadcast(intent);

        NotificationManager notificationManager = (NotificationManager)serviceInstance.getSystemService(Context.NOTIFICATION_SERVICE);
        if(m == MainActivity.MSG_STOPPED) {
            notificationManager.notify(1, buildNotification(true));
        } else if(m == MainActivity.MSG_STARTED) {
            notificationManager.notify(1, buildNotification(false));
        }
    }

    /*void sendPrintoutBroadcastMessage(String message) {
        Intent intent = new Intent(PrintoutActivity.BROADCAST_EVENT_NAME);
        intent.putExtra("newText", message);
        //LocalBroadcastManager.getInstance((Context)serviceInstance).sendBroadcast(intent);
        sendBroadcast(intent);
    }*/

    // dp/sp/px conversion by https://stackoverflow.com/questions/29664993/how-to-convert-dp-px-sp-among-each-other-especially-dp-and-sp
    // and https://stackoverflow.com/questions/6263250/convert-pixels-to-sp
    public static int dpToPx(float dp, Context context) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics());
    }

    public static int spToPx(float sp, Context context) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, sp, context.getResources().getDisplayMetrics());
    }

    public static int dpToSp(float dp, Context context) {
        return (int) (dpToPx(dp, context) / context.getResources().getDisplayMetrics().scaledDensity);
    }

    public static float pxToSp(float px, Context context) {
        float scaledDensity = context.getResources().getDisplayMetrics().scaledDensity;
        return px/scaledDensity;
    }

    // getHeight by https://stackoverflow.com/a/20087258
    public static int getHeight(Context context, CharSequence text, int textSize, int deviceWidth, Typeface typeface,int padding) {
        TextView textView = new TextView(context);
        textView.setPadding(padding,0,padding,padding);
        textView.setTypeface(typeface);
        textView.setText(text, TextView.BufferType.SPANNABLE);
        textView.setTextSize(TypedValue.COMPLEX_UNIT_SP, textSize);
        int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(deviceWidth, View.MeasureSpec.AT_MOST);
        int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        textView.measure(widthMeasureSpec, heightMeasureSpec);
        return textView.getMeasuredHeight();
    }

    int getNumLines() {
        return prefs.getInt("numLines", 2);
    }

    void setNumLines(int numLines) {
        SharedPreferences.Editor ed = prefs.edit();
        ed.putInt("numLines", numLines);
        ed.apply();
    }

    DisplayMetrics mGetDisplayMetrics() {
        WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);
        return displayMetrics;
    }

    int getScreenWidth2() {
        WindowManager windowManager = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        DisplayMetrics displayMetrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);
        return displayMetrics.widthPixels;
        //Point point = new Point();
        //display.getRealSize(point);
        //return point.x;
    }

    int getScreenHeight2() {
        WindowManager windowManager = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        DisplayMetrics displayMetrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);
        return displayMetrics.heightPixels;
        //Point point = new Point();
        //display.getRealSize(point);
        //return point.y;
    }

    int getScreenWidth() {
        WindowManager windowManager = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        Point point = new Point();
        display.getRealSize(point);
        return point.x;
    }

    int getScreenHeight() {
        WindowManager windowManager = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        Point point = new Point();
        display.getRealSize(point);
        return point.y;
    }

    int getTextViewMaxY() {
        synchronized (layoutLock) {
            if(layout == null || layoutParams == null)
                return 0;

            return getTextViewMaxY(layoutParams.height);
        }
    }

    int getTextViewMaxY(int layoutHeight) {
        //DisplayMetrics dm = mGetDisplayMetrics();
        //return (dm.heightPixels - layoutHeight);
        return (getScreenHeight() - layoutHeight);
    }

    void resetLayoutSize() {
        Log.i("UHO_Captions", "ScreenWidth="+getScreenWidth()+" ScreenHeight="+getScreenHeight());

        int paddingPix = (int)((float)dpToPx(16, this));    // todo not hardcode

        int maxTextViewWidthDp = isTablet(getApplicationContext())?700:500;
        Log.i("UHO_Captions", "resetLayoutSize: maxTextViewWidthDp = " + maxTextViewWidthDp);

        WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        int widthMarginLR = dpToPx(10.0f, getApplicationContext());
        int widthPix = getScreenWidth() - widthMarginLR * 2;

        widthPix = Math.min(dpToPx(maxTextViewWidthDp, getApplicationContext()), widthPix);

        int fontSetting = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
        if(fontSetting < 0 || fontSetting >= SettingsActivity.fontSizes.length)  // clip just in case
            fontSetting = 1;
        int heightSp = (getNumLines()+1) * SettingsActivity.fontSizes[fontSetting]; // height of TextView should be 3 rows (2 rows for text, 1 for padding)
        int heightPix = (int)((float)dpToPx(heightSp, this) * getResources().getConfiguration().fontScale);

        heightPix += 2*paddingPix;

        int bottomMarginSp = SettingsActivity.fontSizes[fontSetting];
        int bottomMarginPix = (int)((float)dpToPx(bottomMarginSp, this) * getResources().getConfiguration().fontScale);

        synchronized (layoutLock) {
            if(layout == null || layoutParams == null)
                return;

            layoutParams.width = widthPix;
            layoutParams.height = heightPix;

            layoutParams.gravity = Gravity.BOTTOM | Gravity.LEFT;
            layoutParams.x = (getScreenWidth() - widthPix) / 2;
            layoutParams.y = bottomMarginPix;
        }
    }

    void createLayout() {
        WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        LayoutInflater layoutInflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View overlayView = layoutInflater.inflate(R.layout.activity_transcription, null);

        synchronized (layoutLock) {
            layout = overlayView;

            layoutParams = new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                    WindowManager.LayoutParams.FLAG_FULLSCREEN |
                    //WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS |
                    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN, //|
                    //WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL |
                    //WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                    PixelFormat.TRANSLUCENT
            );
            layoutParams.gravity = Gravity.BOTTOM | Gravity.LEFT;

            // We set opacity to 0.8 (Maximum obscuring opacity): it's a MUST in order for FLAG_NOT_TOUCHABLE to work! (at least for Android Version S forward)
            // (see docs https://developer.android.com/reference/android/view/WindowManager.LayoutParams#FLAG_NOT_TOUCHABLE)
            // According to docs, an alternative would be to use TYPE_ACCESSIBILITY_OVERLAY, however for captions 0.8 is fine.
            layoutParams.alpha = layoutDefaultAlpha;
        }

        resetLayoutSize();

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                synchronized (layoutLock) {
                    if(layoutParams == null)
                        return;

                    windowManager.addView(overlayView, layoutParams);
                }
                TextView textView = overlayView.findViewById(R.id.textview_transcription);
                textView.setOnTouchListener(textViewTouchListener);
                resetLayout(true);  // this time we pass true, because it's first execution
            }
        });
        //return overlayView;
    }

    class TextTouchListener implements View.OnTouchListener {
        float downX, downY;
        WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);

        class GestureTap extends GestureDetector.SimpleOnGestureListener {
            @Override
            public boolean onSingleTapConfirmed(@NonNull MotionEvent e) {
                Log.i("UHO_Captions", "Tap detected!");

                // Toggle number of lines
                if(getNumLines() == SettingsActivity.numLinesSmall)
                    setNumLines(SettingsActivity.numLinesLarge);
                else
                    setNumLines(SettingsActivity.numLinesSmall);

                resetLayout(false);

                // TODO: reset layout, etc, toggle num lines, don't hardcore 2 and 5
                return true; // consume event
                //return super.onSingleTapConfirmed(e);
            }
        }

        /*class GestureScale extends ScaleGestureDetector.SimpleOnScaleGestureListener {
            float scaleAtUpdateFont = 1.0f;
            float increaseScaleUpdate = 1.10f;    // bigger value means more textview zoom needed to change font
            float decreaseScaleUpdate = 0.9f;

            @Override
            public boolean onScaleBegin(@NonNull ScaleGestureDetector detector) {
                scaleAtUpdateFont = 1.0f;
                return true;
                //return super.onScaleBegin(detector);
            }

            @Override
            public void onScaleEnd(@NonNull ScaleGestureDetector detector) {
                scaleAtUpdateFont = 1.0f;
                super.onScaleEnd(detector);
            }

            @Override
            public boolean onScale(@NonNull ScaleGestureDetector detector) {
                Log.i("UHO_Captions", String.format("Scale factor %.2f detected", detector.getScaleFactor()));

                float scale = detector.getScaleFactor();
                scaleAtUpdateFont = scaleAtUpdateFont * scale;
                if(scaleAtUpdateFont >= increaseScaleUpdate) {
                    updateFont(true);
                    scaleAtUpdateFont = 1.0f;
                } else if(scaleAtUpdateFont <= decreaseScaleUpdate){
                    updateFont(false);
                    scaleAtUpdateFont = 1.0f;
                }

                return true;
                //return super.onScale(detector);
            }

            void updateFont(boolean increase) {
                int fontSettingMin = 0;
                int fontSettingMax = SettingsActivity.fontSizes.length - 1;

                int fontSetting = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
                int newFontSetting = Math.clamp((increase)?(fontSetting + 1):(fontSetting - 1), fontSettingMin, fontSettingMax);
                if(newFontSetting != fontSetting) {
                    SharedPreferences.Editor ed = prefs.edit();
                    ed.putInt("fontSetting", newFontSetting);
                    ed.apply();
                }
            }

        }*/

        GestureTap gestureTap;
        GestureDetector gestureDetector;
        ScaleGestureDetector scaleGestureDetector;
        //GestureScale gestureScale;

        TextTouchListener(Context context) {
            gestureTap = new GestureTap();
            //gestureScale = new GestureScale();
            gestureDetector = new GestureDetector(context, gestureTap);
            //scaleGestureDetector = new ScaleGestureDetector(context, gestureScale);
        }

        /*
        // Commented on 20.9.2026
        float dhPrev = -1.0f;
        @Override
        public boolean onTouch(View view, MotionEvent motionEvent) {
            if(motionEvent.getPointerCount() == 2) {
                float dx = motionEvent.getX(0) - motionEvent.getX(1);
                float dy = motionEvent.getY(0) - motionEvent.getY(1);
                float dh = (float)Math.hypot(dx, dy);

                if(motionEvent.getActionMasked() == MotionEvent.ACTION_MOVE) {
                    if(dhPrev > 0.0f) {
                        float scaleFactor = dh / dhPrev;
                        precisionOnScale(scaleFactor);
                    }
                }
                dhPrev = dh;
                //scaleGestureDetector.onTouchEvent(motionEvent);
                //onTouchPinch(motionEvent);
                //Log.i("UHO_Captions", String.format("pinchFactor = %.2f", pinchFactor));
            } else {
                dhPrev = -1.0f; // reset scale factors if motions is not with 2 pointers
                if (motionEvent.getPointerCount() == 1) {
                    onTouchMove(motionEvent);
                    gestureDetector.onTouchEvent(motionEvent);
                }
            }
            return true;
        }*/

        float scaleAtUpdateFont = 1.0f;
        float increaseScaleUpdate = 1.2f;    // bigger value means more textview zoom needed to change font
        float decreaseScaleUpdate = 0.85f;

        public void precisionOnScale(float scale) {
            Log.i("UHO_Captions", String.format("Scale factor %.5f detected", scale));

            scaleAtUpdateFont = scaleAtUpdateFont * scale;
            if(scaleAtUpdateFont >= increaseScaleUpdate) {
                updateFont(true);
                scaleAtUpdateFont = 1.0f;
            } else if(scaleAtUpdateFont <= decreaseScaleUpdate){
                updateFont(false);
                scaleAtUpdateFont = 1.0f;
            }
        }

        // added on 20.9.2026 (most code from UHO Global - deaf transcribe app)
        float dhPrev = -1.0f;
        boolean multiTouch = false;
        @Override
        public boolean onTouch(View v, MotionEvent event) {
            int action = event.getActionMasked();
            int pointerCount = event.getPointerCount();

            if (pointerCount == 2) {
                float dx = event.getX(0) - event.getX(1);
                float dy = event.getY(0) - event.getY(1);
                float dh = (float) Math.hypot(dx, dy);

                if (action == MotionEvent.ACTION_MOVE) {
                    if (dhPrev > 0.0f) {
                        float scaleFactor = dh / dhPrev;
                        precisionOnScale(scaleFactor);
                    }
                }
                dhPrev = dh;
                //scaleGestureDetector.onTouchEvent(motionEvent);
                //onTouchPinch(motionEvent);
                //Log.i("UHO_Transcribe", String.format("pinchFactor = %.2f", pinchFactor));
            }


            if (action == MotionEvent.ACTION_POINTER_DOWN && pointerCount >= 2) {
                if (!multiTouch) {
                    multiTouch = true;

                    MotionEvent cancelEvent = MotionEvent.obtain(
                            event.getDownTime(),
                            event.getEventTime(),
                            MotionEvent.ACTION_CANCEL,
                            event.getX(),
                            event.getY(),
                            0);

                    v.dispatchTouchEvent(cancelEvent);
                    cancelEvent.recycle();
                }
            }

            if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP)
                multiTouch = false;

            //if (multiTouch || pointerCount >= 2)
            //    return true;

            if (pointerCount == 1) {
                onTouchMove(event);
                gestureDetector.onTouchEvent(event);
            }

            return true; // consume events
        }

        void updateFont(boolean increase) {
            int fontSettingMin = 0;
            int fontSettingMax = SettingsActivity.fontSizes.length - 1;

            int fontSetting = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
            int newFontSetting = Math.clamp((increase)?(fontSetting + 1):(fontSetting - 1), fontSettingMin, fontSettingMax);
            if(newFontSetting != fontSetting) {
                SharedPreferences.Editor ed = prefs.edit();
                ed.putInt("fontSetting", newFontSetting);
                ed.apply();
            }
        }

        /*void onTouchPinch(MotionEvent motionEvent) {
            float pinchFactor = motionEvent.getAxisValue(MotionEvent.AXIS_GESTURE_PINCH_SCALE_FACTOR);
            switch (motionEvent.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = motionEvent.getRawX();
                    downY = motionEvent.getRawY();
                    break;
                case MotionEvent.ACTION_MOVE:
                    float moveX = motionEvent.getRawX();
                    float moveY = motionEvent.getRawY();
                    float dx = moveX - downX;
                    float dy = moveY - downY;

                    //view.setX(view.getX() + dx);
                    //view.setY(view.getY() + dy);
                    //params.x += (int)dx;
                    params.y -= (int)dy;
                    windowManager.updateViewLayout(layout, params);

                    downX = moveX;
                    downY = moveY;
                    break;
                case MotionEvent.ACTION_UP:
                    break;
        }*/

        void onTouchMove(MotionEvent motionEvent) {
            switch (motionEvent.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = motionEvent.getRawX();
                    downY = motionEvent.getRawY();
                    break;
                case MotionEvent.ACTION_MOVE:
                    float moveX = motionEvent.getRawX();
                    float moveY = motionEvent.getRawY();
                    float dx = moveX - downX;
                    float dy = moveY - downY;

                    //view.setX(view.getX() + dx);
                    //view.setY(view.getY() + dy);
                    //params.x += (int)dx;
                    synchronized (layoutLock) {
                        if(layout == null || layoutParams == null)
                            break;

                        layoutParams.y -= (int) dy;
                        layoutParams.y = Math.clamp(layoutParams.y, 0, getTextViewMaxY());
                        //Log.i("UHO_Captions", "UPDATEVIEWLAYOUT");
                        windowManager.updateViewLayout(layout, layoutParams);
                    }

                    downX = moveX;
                    downY = moveY;
                    break;
                case MotionEvent.ACTION_UP:
                    break;
            }
        }
    };

    /*View.OnTouchListener textViewTouchListener = new View.OnTouchListener() {
        float downX, downY;
        WindowManager windowManager;
        WindowManager.LayoutParams params;

        @Override
        public boolean onTouch(View view, MotionEvent motionEvent) {
            if(layout == null)
                return true;

            windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
            params = (WindowManager.LayoutParams) layout.getLayoutParams();
            //ViewGroup.LayoutParams params = view.getLayoutParams();

            float pinchFactor = motionEvent.getAxisValue(MotionEvent.AXIS_GESTURE_PINCH_SCALE_FACTOR);

            if(motionEvent.getPointerCount() == 1) {
                onTouchMove(motionEvent);
            } else if(motionEvent.getPointerCount() == 2) {
                Log.i("UHO_Captions", String.format("pinchFactor = %.2f", pinchFactor));
            }

            return true;
            //return false;
        }

        void onTouchMove(MotionEvent motionEvent) {
            switch (motionEvent.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = motionEvent.getRawX();
                    downY = motionEvent.getRawY();
                    break;
                case MotionEvent.ACTION_MOVE:
                    float moveX = motionEvent.getRawX();
                    float moveY = motionEvent.getRawY();
                    float dx = moveX - downX;
                    float dy = moveY - downY;

                    //view.setX(view.getX() + dx);
                    //view.setY(view.getY() + dy);
                    //params.x += (int)dx;
                    params.y -= (int)dy;
                    windowManager.updateViewLayout(layout, params);

                    downX = moveX;
                    downY = moveY;
                    break;
                case MotionEvent.ACTION_UP:
                    break;
            }
        }
    };*/

    enum MainMessage {
        stopThread,
        beginMic,
        beginMedia,
        beginPhone,
    }

    Queue<MainMessage> mainMessageQueue = new PriorityQueue<>();

    /*
     orientationEventListener = new OrientationEventListener(getApplicationContext()) {
     @Override public void onOrientationChanged(int i) {
                Log.i("UHO_Captions", "Orientation changed!!");
                WindowManager.LayoutParams params = (WindowManager.LayoutParams)layout.getLayoutParams();
                resetLayoutSize(params);
                WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
                windowManager.updateViewLayout(layout, params);
            }
        };

        if (orientationEventListener.canDetectOrientation())
            orientationEventListener.enable();
     */

    void resetLayout(boolean orientationChanged) {
        synchronized (layoutLock) {
            if (layout == null || layoutParams == null)
                return;
        }

        int delayMs = orientationChanged?500:0;

        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                // save previous width/height and reset layout
                int prevWidth;
                int prevX;
                int prevY;

                synchronized (layoutLock) {
                    if(layout == null || layoutParams == null)
                        return;

                    prevWidth = layoutParams.width;
                    prevX = layoutParams.x;
                    prevY = layoutParams.y;
                }

                resetLayoutSize();

                WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
                TextView textView;

                // if screen wasn't rotated, keep old x and y positions of the textview
                synchronized (layoutLock) {
                    if(layout == null || layoutParams == null)
                        return;

                    if (!orientationChanged/* || prevWidth == 0*/) {
                        Log.i("UHO_Captions", "Changed x and y");
                        layoutParams.x = prevX;
                        layoutParams.y = prevY;
                    } else {
                        // if only orientation changed, but captions were previously invisible, keep them invisible
                        if (prevWidth == 0) {
                            layoutParams.width = 0;
                            layoutParams.height = 0;
                        }
                    }

                    layoutParams.y = Math.clamp(layoutParams.y, 0, getTextViewMaxY(layoutParams.height));
                    Log.i("UHO_Captions", "Setting params: width=" + layoutParams.width + ", height=" + layoutParams.height + ", x=" + layoutParams.x);
                    windowManager.updateViewLayout(layout, layoutParams);

                    textView = layout.findViewById(R.id.textview_transcription);
                }

                textView.setLines(getNumLines());
                textView.setMaxLines(getNumLines());
                int fontSetting = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
                if(fontSetting < 0 || fontSetting >= SettingsActivity.fontSizes.length)  // clip just in case
                    fontSetting = 1;
                textView.setTextSize(
                        TypedValue.COMPLEX_UNIT_PX,
                        SettingsActivity.fontSizes[fontSetting] * getResources().getDisplayMetrics().scaledDensity
                );

                Log.i("UHO_Captions", "setting font size");
            }
        }, delayMs);
    }

    void showCaptions(boolean show) {
        synchronized (layoutLock) {
            if (layout == null || layoutParams == null)
                return;
        }

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);

                synchronized (layoutLock) {
                    if(layout == null || layoutParams == null)
                        return;

                    if (show) {
                        if (layoutParams.width == 0 && layoutParams.height == 0)
                            resetLayout(false);
                    } else {
                        if (layoutParams.width > 0 && layoutParams.height > 0) {
                            layoutParams.width = 0;
                            layoutParams.height = 0;
                            Log.i("UHO_Captions", "UPDATEVIEWLAYOUT");
                            windowManager.updateViewLayout(layout, layoutParams);
                        }
                    }
                }
            }
        });
    }

    void showCaptions2(boolean show) {
        synchronized (layoutLock) {
            if(layout == null || layoutParams == null)
                return;

            if(!show) {
                if(layoutParams.width > 0 || layoutParams.height > 0)
                    showCaptions(false);
            } else {
                if(layoutParams.width == 0 || layoutParams.height == 0)
                    showCaptions(true);
            }
        }
    }

    void showCaptions3(boolean show) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                synchronized (layoutLock) {
                    if(layout == null || layoutParams == null)
                        return;

                    WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);

                    if(show) {
                        layout.setVisibility(View.VISIBLE);
                        if(layout.getLayoutParams().width == 1) {
                            resetLayoutSize();
                            layoutParams.alpha = layoutDefaultAlpha;
                            windowManager.updateViewLayout(layout, layoutParams);
                        }
                    }
                    else {
                        layoutParams.width = 1;
                        layoutParams.height = 1;
                        layoutParams.x = 0;
                        layoutParams.y = 0;
                        layoutParams.alpha = 0.0f;
                        windowManager.updateViewLayout(layout, layoutParams);
                    }
                }
            }
        });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.i("UHO_Captions", "Configuration changed!");
        resetLayout(true);
    }

    enum MainThreadState {

    }

    //Queue<MainThreadMsg> mainThreadQueue = new ConcurrentLinkedQueue<>();

    Runnable mainThreadRunnable = new Runnable() {
        //OrientationEventListener orientationEventListener;

        void cleanup() {
            synchronized (layoutLock) {
                if (layout != null) {
                    WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
                    TextView textView = layout.findViewById(R.id.textview_transcription);
                    textView.setOnTouchListener(null);
                    windowManager.removeView(layout);
                    layout = null;
                }

                if (layoutParams != null)
                    layoutParams = null;
            }

            //if(orientationEventListener != null)
            //    orientationEventListener.disable();
            mediaProjection.stop();
            mediaProjection = null;
            mainThread = null;
            mainThreadStopSignal = false;

            sendMainActivityBroadcastMessage(MainActivity.MSG_FINISHED);
        }

        AudioRecord initAudioRecord(boolean useMicrophone) {
            AudioRecord audioRecord;

            if (ActivityCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
                return null;

            int sampleRate = 16000;
            int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;
            int channelConfig = AudioFormat.CHANNEL_IN_MONO;
            int minBufferSize = 2 * AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
            int targetBufferSize = Math.max(8192 * Float.SIZE / Byte.SIZE, minBufferSize);

            if(!useMicrophone) {
                // Use media projection to record sound from other apps
                // These are most likely the only valid usages for non-system apps
                AudioPlaybackCaptureConfiguration config = new AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                        .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                        .addMatchingUsage(AudioAttributes.USAGE_GAME)
                        .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                        .build();

                audioRecord = new AudioRecord.Builder()
                        .setAudioFormat(new AudioFormat.Builder()
                                .setSampleRate(sampleRate)
                                .setEncoding(audioFormat)
                                .setChannelMask(channelConfig)
                                .build())
                        //.setAudioSource(MediaRecorder.AudioSource.DEFAULT)    <-- use this when not using Playback Capture Config
                        .setAudioPlaybackCaptureConfig(config)
                        .setBufferSizeInBytes(targetBufferSize)
                        .build();
            } else {
                // Record from microphone
                // ("VOICE_RECOGNITION" has noise suppression and allows for recording during phone call having an accessibility service)
                audioRecord = new AudioRecord(
                        MediaRecorder.AudioSource.VOICE_RECOGNITION, //MediaRecorder.AudioSource.MIC,
                        sampleRate,
                        channelConfig,
                        audioFormat,
                        targetBufferSize);
            }

            return audioRecord;
        }

        boolean shouldUseMicrophone(AudioManager manager) {
            boolean useMic = false;

            int mode = manager.getMode();
            if(mode == AudioManager.MODE_RINGTONE ||
                    mode == AudioManager.MODE_IN_CALL ||
                    mode == AudioManager.MODE_IN_COMMUNICATION ||
                    mode == AudioManager.MODE_CALL_SCREENING ||
                    mode == AudioManager.MODE_CALL_REDIRECT ||
                    mode == AudioManager.MODE_COMMUNICATION_REDIRECT)
                useMic = true;

            return useMic;
        }

        @Override
        public void run() {
            // mainThreadQueue.clear();

            // The code will assume that if we run using media projection, we already have a valid token (mediaProjection variable)
            //boolean useMicrophone = prefs.getBoolean("useMicrophone", SettingsActivity.defaultUseMicrophone);
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO); //THREAD_PRIORITY_FOREGROUND);

            sendMainActivityBroadcastMessage(MainActivity.MSG_STARTED);

            Log.i("UHO_Captions", "Creating window");
            createLayout();
            Log.i("UHO_Captions", "Created");

            /*if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
                cleanup();
                sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                return;
            }*/

            if (ActivityCompat.checkSelfPermission(getApplicationContext(), android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                nativeStopASR();
                cleanup();
                sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                return;
            }

            //AssetManager assetManager = getResources().getAssets();
            AssetManager assetManager = ((App)getApplication()).getAssetManager();

            Log.i("UHO_Captions", String.format("assetManager is %s", (assetManager==null)?"null":"OK"));
            
            Log.i("UHO_Captions", "Starting...");
            // ASR start

            // Find out which language we're starting ASR for
            SharedPreferences preferences = getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);
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
                Context applicationContext = getApplicationContext();
                AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);
                AssetPackLocation location = assetPackManager.getPackLocation(chosenLangInfo.assetName);
                if(location == null) {
                    nativeStopASR();
                    cleanup();
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
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
            Log.i("UHO_Captions", "Calling nativeStartASR:" + "\n" + fullAsrModelPath+ "\n" + fullAsrVocabPath + "\n" + fullPncModelPath + "\n" + fullPncVocabPath);
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
            Log.i("UHO_Captions", "Start!");

            //String fullText = "";
            //Long lastCallbackTime = System.currentTimeMillis();

            int truncIndex = 0;
            String truncFullText = "";
            String prevTruncFullText = "";
            long lastSoundTime = System.currentTimeMillis();

            boolean useMicrophone;
            boolean prevUseMicrophone;
            AudioRecord audioRecord = null;
            AudioManager audioManager = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
            useMicrophone = shouldUseMicrophone(audioManager);
            prevUseMicrophone = !useMicrophone; // this will trigger first initAudioRecord
            Log.i("UHO_Captions", "useMicrophone = " + useMicrophone);

            boolean firstTimeDiff = false; //true;

            int prevFontSize, currFontSize;
            prevFontSize = currFontSize = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
            while (!mainThreadStopSignal) {
                currFontSize = prefs.getInt("fontSetting", SettingsActivity.defaultFontSetting);
                if(currFontSize != prevFontSize)
                    resetLayout(false);
                prevFontSize = currFontSize;

                useMicrophone = shouldUseMicrophone(audioManager);
                if(useMicrophone != prevUseMicrophone) {
                    // stop recording if its already in action
                    if(audioRecord != null && audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
                        Log.i("UHO_Captions", "Calling stopRecording");
                        audioRecord.stop();
                    }

                    try {
                        audioRecord = initAudioRecord(useMicrophone);
                    } catch (Exception e) {
                        nativeStopASR();
                        cleanup();
                        sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                        return;
                    }

                    if(audioRecord == null) {   // added null check
                        nativeStopASR();
                        cleanup();
                        sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                        return;
                    }

                    Log.i("UHO_Captions", "Calling startRecording");
                    try {
                        audioRecord.startRecording();
                    } catch (IllegalStateException e) {
                        Log.i("UHO_Captions", "Exception message: " + Objects.requireNonNull(e.getMessage()));
                        nativeStopASR();
                        cleanup();
                        sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                        return;
                    }
                }
                prevUseMicrophone = useMicrophone;

                if(audioRecord == null) {
                    nativeStopASR();
                    cleanup();
                    sendMainActivityBroadcastMessage(MainActivity.MSG_STARTERROR);
                    return;
                }

                float[] buff = new float[800];
                int numRead = audioRecord.read(buff, 0, buff.length, AudioRecord.READ_BLOCKING);
                if(numRead < 0) {
                    break;  // exit loop and stop all
                } else if(numRead == 0) {
                    try {
                        Thread.sleep(10);
                        continue;
                    } catch (InterruptedException e) {
                        break;  // exit loop and stop all
                    }
                }

                float[] addBuff = buff;
                if (numRead < buff.length)
                    addBuff = Arrays.copyOfRange(buff, 0, numRead);
                nativeAddNewAudio(addBuff);

                String nativeFullText = nativeGetNewText();
                //Log.i("UHO_Captions", String.format("[mainThreadRunnable] nativeFullText = \"%s\"", nativeFullText));

                // new code
                //if(nativeFullText.isEmpty())
                //    onNewTextReset();
                // ####################


                nativeFullText = nativeFullText.replace("*", "");

                onNewText(nativeFullText);
                nativeFullText = incFullText;

                truncIndex = Math.clamp(truncIndex, 0, nativeFullText.length());
                //Log.i("UHO_Captions", String.format("[mainThreadRunnable] truncIndex = %d", truncIndex));
                truncFullText = nativeFullText.substring(truncIndex);
                //Log.i("UHO_Captions", String.format("[mainThreadRunnable] truncFullText = %s", truncFullText));
                // If 10 seconds of silence, hide captions
                if(!truncFullText.equals(prevTruncFullText))
                    lastSoundTime = System.currentTimeMillis();
                long currTime = System.currentTimeMillis();
                double timeDiff = (double) (currTime - lastSoundTime) / 1000.0;

                if(timeDiff >= 10.0) {
                    truncFullText = "";
                    truncIndex = nativeFullText.length();

                    if(firstTimeDiff) {
                        nativeResetText();  // make sure this isn't called every time timeDiff is >= 10
                        firstTimeDiff = false;
                    }
                    showCaptions3(false);
                } else {
                    if(!firstTimeDiff)
                        firstTimeDiff = true;

                    showCaptions3(true);
                }

                prevTruncFullText = truncFullText;

                MyRunnable myRunnable;

                synchronized (layoutLock) {
                    if(layout == null || layoutParams == null)
                        break;
                    myRunnable = new MyRunnable(truncFullText);
                }
                mHandler.post(myRunnable);
                try {
                    myRunnable.latch.await();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }

                truncIndex += myRunnable.lastNonAlphaCharPos;

                /*
                // Get new text and save into fullText. In case we got empty text for last 10 seconds, clear text.
                String text = nativeGetNewText();
                Long time = System.currentTimeMillis();
                if(!text.isEmpty())
                    lastCallbackTime = time;
                if(time - lastCallbackTime > 10*1000)
                    fullText = "";
                else
                    fullText += text;

                if(PrintoutActivity.active) {
                    fullText = "";  // this hides captions
                    if (!text.isEmpty())
                        sendPrintoutBroadcastMessage(text);
                } else {
                    fullText = fullText.replace("*", "");   // don't display * (used as indicator for newline in printout textview)
                }

                MyRunnable myRunnable = new MyRunnable(fullText, layout);
                mHandler.post(myRunnable);
                try {
                    myRunnable.latch.await();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }

                fullText = myRunnable.myFullText;*/

                //Log.i("UHO_Captions", String.format("Buff[0] = %.4f", buff[0]));
            }

            // Cleanup and exit

            if(audioRecord != null)
                audioRecord.stop();
            nativeStopASR();
            cleanup();
            sendMainActivityBroadcastMessage(MainActivity.MSG_STOPPED);
        }
    };

    void onNewText(String text) {
        long systemTimeCurr = System.nanoTime();
        double elapsedSeconds = (double)(systemTimeCurr - systemTimePrev)/1.0e9;
        systemTimePrev = systemTimeCurr;

        //Log.i("UHO_Captions", "parseNewText");
        onNewTextHandler(text, (float)elapsedSeconds);
    }

    void onNewTextReset() {
        newLen = 0;
        prevLen = 0;
        endIndex = 0;
        systemTimePrev = System.nanoTime();
        incFullText = "";
    }

    /*
    int newLen = 0;
    int prevLen = 0;
    float endIndex = 0.0f;
    long systemTimePrev = System.nanoTime();
    String incFullText = "";
    */

    int newLen = 0;
    int prevLen = 0;
    float prevLettersPerSec = 5.0f;
    float endIndex = 0.0f;
    long systemTimePrev = System.nanoTime();
    String incFullText;

    void onNewTextHandler(String fullText, float timeElapsedSeconds) {
        int minLettersPerSecond = 5;	// config
        int maxLettersPerSecond = 100;	// config
        float normalSecondsPerUpdate = nativeGetTimeASR(); //0.6f;	// config
        float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
        float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

        //Log.i("UHO4", String.format("normalSecondsPerUpdate = %.3f\n", normalSecondsPerUpdate));
        //Log.i("UHO4", String.format("timeElapsedSeconds = %.4f\n", timeElapsedSeconds));

        newLen = fullText.length();
        if (newLen < prevLen) {
            prevLen = newLen;
            endIndex = Math.clamp(endIndex, 0.0f, (float)newLen);
        }

        float lettersPerSec;
        if (newLen == prevLen) {
            //Log.i("UHO4", String.format("newLen = prevLen\n", timeElapsedSeconds));
            lettersPerSec = prevLettersPerSec; //maxLettersPerSecond;
        }
        else {
            float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
            //secondsPerLetter = Math.min(maxSecondsPerLetter, secondsPerLetter);	// in [sec]
            secondsPerLetter = Math.clamp(secondsPerLetter, minSecondsPerLetter, maxSecondsPerLetter);	// in [sec]
            lettersPerSec = 1.0f / secondsPerLetter;
            //Log.i("UHO4", String.format("lettersPerSec = %.3f\n", lettersPerSec));
            prevLettersPerSec = lettersPerSec;
        }

        //int letterIncrement = (int)ceil(timeElapsedSeconds * lettersPerSec);
        float letterIncrement = timeElapsedSeconds * lettersPerSec;
        //Log.i("UHO4", "letterIncrement = " + letterIncrement);

        endIndex += letterIncrement;

        if (endIndex >= (float)prevLen) {
            prevLen = newLen;
        }
        //Log.i("UHO4", "endIndex = " + endIndex);
        //Log.i("UHO4", "prevLen=" + prevLen + "  newLen=" + newLen);

        endIndex = Math.clamp(endIndex, 0.0f, (float)newLen);

        int tmp = (int)endIndex;
        incFullText = fullText.substring(0, tmp);
    }

    /*void onNewTextHandler(String fullText, float timeElapsedSeconds) {
        int minLettersPerSecond = 5;	// config
        int maxLettersPerSecond = 100;	// config
        float normalSecondsPerUpdate = nativeGetTimeASR(); //0.6f;	// config
        float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
        float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

        Log.i("UHO_Captions", String.format("normalSecondsPerUpdate = %.3f\n", normalSecondsPerUpdate));
        Log.i("UHO_Captions", String.format("timeElapsedSeconds = %.4f\n", timeElapsedSeconds));

        newLen = fullText.length();
        if (newLen < prevLen) {
            prevLen = newLen;
            endIndex = (float)newLen;
        }

        float lettersPerSec;
        if (newLen == prevLen) {
            lettersPerSec = maxLettersPerSecond;
        }
        else {
            float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
            //secondsPerLetter = Math.min(maxSecondsPerLetter, secondsPerLetter);	// in [sec]
            secondsPerLetter = Math.clamp(secondsPerLetter, minSecondsPerLetter, maxSecondsPerLetter);
            lettersPerSec = 1.0f / secondsPerLetter;
            Log.i("UHO_Captions", String.format("lettersPerSec = %.3f\n", lettersPerSec));
        }

        //int letterIncrement = (int)ceil(timeElapsedSeconds * lettersPerSec);
        float letterIncrement = timeElapsedSeconds * lettersPerSec;
        //Log.i("UHO_Captions", "letterIncrement = " + letterIncrement);

        endIndex += letterIncrement;
        if (endIndex >= (float)newLen) {
            endIndex = (float)newLen;
            prevLen = newLen;
        }

        int tmp = Math.clamp((int)endIndex, 0, (int)fullText.length());
        incFullText = fullText.substring(0, tmp);
    }*/
    /*void onNewTextHandler(String fullText, float timeElapsedSeconds) {
        int minLettersPerSecond = 10;	// config
        int maxLettersPerSecond = 100;	// config
        float normalSecondsPerUpdate = nativeGetTimeASR(); //0.6f;	// config
        float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
        float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

        newLen = fullText.length();
        if (newLen <= prevLen) {
            prevLen = newLen;
            endIndex = newLen;
        }

        float lettersPerSec;
        if (newLen == prevLen) {
            lettersPerSec = minSecondsPerLetter;
        }
        else {
            float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
            secondsPerLetter = Math.min(maxSecondsPerLetter, secondsPerLetter);	// in [sec]
            lettersPerSec = 1.0f / secondsPerLetter;
        }
        //wprintf(L"lettersPerSec = %.3f\n", lettersPerSec);

        int letterIncrement = (int)ceil(timeElapsedSeconds * lettersPerSec);
        //wprintf(L"letterIncrement = %i\n", letterIncrement);

        endIndex += letterIncrement;
        if (endIndex >= newLen) {
            endIndex = newLen;
            prevLen = newLen;
        }

        int tmp = Math.clamp(endIndex, 0, (int)fullText.length());
        incFullText = fullText.substring(0, tmp);
    }*/

    public boolean isTablet(Context context) {  // suggested by GPT
        return (context.getResources().getConfiguration().screenLayout
                & Configuration.SCREENLAYOUT_SIZE_MASK)
                >= Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent accessibilityEvent) {

    }

    @Override
    public void onInterrupt() {

    }

    int numRemoveLast = 0;

    // Runnable that truncates full captions' text to 2 lines and updates the textView
    public class MyRunnable implements Runnable {
        int lastNonAlphaCharPos = 0;
        int lastNonAlphaCharPos2 = 0;
        String myFullText;
        CountDownLatch latch = new CountDownLatch(1);

        public MyRunnable(String myFullText) {
            super();
            this.myFullText = myFullText;
        }

        public boolean isChinese(int codePoint) {
            if (!Character.isValidCodePoint(codePoint))
                return false;

            Character.UnicodeScript script = Character.UnicodeScript.of(codePoint);
            return script == Character.UnicodeScript.HAN;
        }

        public boolean isJapanese(int codePoint) {
            if (!Character.isValidCodePoint(codePoint))
                return false;

            Character.UnicodeScript script = Character.UnicodeScript.of(codePoint);
            return script == Character.UnicodeScript.HIRAGANA
                    || script == Character.UnicodeScript.KATAKANA
                    || script == Character.UnicodeScript.HAN;
        }

        // Thai uses a single dedicated Unicode script
        public boolean isThai(int codePoint) {
            if (!Character.isValidCodePoint(codePoint))
                return false;

            return Character.UnicodeScript.of(codePoint) == Character.UnicodeScript.THAI;
        }

        public boolean isKorean(int codePoint) {
            if (!Character.isValidCodePoint(codePoint))
                return false;

            Character.UnicodeScript script = Character.UnicodeScript.of(codePoint);
            return (script == Character.UnicodeScript.HANGUL);
        }

        public boolean isKoreanWithHan(int codePoint)
        {
            if (!Character.isValidCodePoint(codePoint))
                return false;

            Character.UnicodeScript script = Character.UnicodeScript.of(codePoint);
            return (script == Character.UnicodeScript.HANGUL || script == Character.UnicodeScript.HAN);
        }

        // this is used
        public boolean isUnspacedCharLanguage(int c) {
            if (!Character.isValidCodePoint(c))
                return false;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                // for >= API LEVEL 33 (Android TIRAMISU) don't need to include Korean
                // because we use android:lineBreakWordStyle="phrase" & android:lineBreakStyle="strict"
                // inside activity_transcription.xml (those two XML attributes work only on >= 33)
                // and make sure that 2 line captions don't break Korean text mid-word
                return (isChinese(c) || isJapanese(c) || isThai(c));
            }
            else {
                // include Korean here, because TextView doesn't do line breaks at word level for Korean but at character level
                return (isChinese(c) || isJapanese(c) || isThai(c) || isKoreanWithHan(c));
            }
        }

        @Override
        public void run() {
            TextView textView;
            int layoutParamsWidth;
            synchronized (layoutLock) {
                if(layout == null || layoutParams == null)
                    return;

                textView = layout.findViewById(R.id.textview_transcription);
                layoutParamsWidth = layoutParams.width;
            }

            textView.setText(this.myFullText);

            int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(layoutParamsWidth, View.MeasureSpec.EXACTLY);
            int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
            textView.measure(widthMeasureSpec, heightMeasureSpec);

            //Log.i("UHO_Captions", String.format("getNumLines=%d, textView.getLineCount=%d\n", getNumLines(), textView.getLineCount()));

            if(textView.getLineCount() > getNumLines()) {
                Log.i("UHO_Captions", "LINE COUNT > getNumLines()!");
                lastNonAlphaCharPos = 0;    // added this on 24.8.2026

                for(int i = 0; i < this.myFullText.length(); i++) {
                    String subText = this.myFullText.substring(0, i + 1);
                    textView.setText(subText);
                    textView.measure(widthMeasureSpec, heightMeasureSpec);

                    int c = subText.codePointAt(subText.length() - 1);
                    if (!Character.isAlphabetic(c) || isUnspacedCharLanguage(c))
                        lastNonAlphaCharPos = i;

                    //Log.i("UHO_Captions", String.format("at %d, isUnspacedCharLanguage is %s", i, isUnspacedCharLanguage(subText.codePointAt(subText.length() - 1))?"TRUE":"FALSE"));

                    if(textView.getLineCount() >= 2)
                        break;
                }

                this.myFullText = this.myFullText.substring(lastNonAlphaCharPos);
            }

            textView.setText(this.myFullText);
            this.latch.countDown();
        }
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
    public native void nativeStopASR();

    public native void nativeAddNewAudio(float[] audio);
    public native String nativeGetNewText();
    public native void nativeResetText();
    public native float nativeGetTimeASR();
}
