// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_transcribe;

import static android.text.Html.FROM_HTML_MODE_COMPACT;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.text.HtmlCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.ViewModelProvider;

import android.Manifest;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.WebSettings;
import android.webkit.WebView;

import android.widget.TextView;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewException;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;
import com.google.android.play.core.review.model.ReviewErrorCode;
import com.obstino.uho_transcribe.databinding.ActivityMainBinding;

import org.json.JSONObject;

public class TranscribeActivity extends AppCompatActivity {
    MainViewModel viewModel;

    WebView webview;
    Double fontSizeEm = 2.0;
    Double fontSizeStep = 0.2;
    Double minFontSize = 1.0;
    Integer numLinesBottomScroll = 3;   // when we're at most 3 lines before bottom of page, autoscroll down

    boolean changedFontUpdate = false;

    String backgroundColorLight = "#ffffff";
    String textColorErrorLight = "red";
    String textColorInitLight = "gray";
    String textColorSpeechLight = "black";

    String backgroundColorDark = "#121212";
    String textColorErrorDark = "red";
    String textColorInitDark = "lightgray";
    String textColorSpeechDark = "white";

    /*
    // Used to load the 'uho' library on application startup.
    static {
        System.loadLibrary("uho");
    }
     */

    private ActivityMainBinding binding;
    final int REQUEST_PERMISSION_MICROPHONE = 100;

    static final int MSG_LOADING = 0;
    static final int MSG_STARTED = 1;
    static final int MSG_STARTERROR = 2;

    boolean isTouchActive;

    String getInitColor() {
        return darkModeEnabled?textColorInitDark:textColorInitLight;
    }

    String getSpeechColor() {
        return darkModeEnabled?textColorSpeechDark:textColorSpeechLight;
    }

    String getErrorColor() {
        return darkModeEnabled?textColorErrorDark:textColorErrorLight;
    }

    String getBackgroundColor() {
        return darkModeEnabled?backgroundColorDark:backgroundColorLight;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //EdgeToEdge.enable(this);
        setContentView(R.layout.activity_transcribe);
        /*ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });*/

        initialChangeDarkModeState();
        /*
        // Calculate ActionBar height
        TypedValue tv = new TypedValue();
        if (getTheme().resolveAttribute(android.R.attr.actionBarSize, tv, true))
        {
            int actionBarHeight = TypedValue.complexToDimensionPixelSize(tv.data,getResources().getDisplayMetrics());
            //Log.i("UHO_Transcribe", String.format("actionBarHeight = %d", actionBarHeight));
            findViewById(R.id.textview_tmp).setPadding(0, actionBarHeight, 0, 0);
        }*/

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_transcribe);
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //onBackPressed();
                getOnBackPressedDispatcher().onBackPressed();
            }
        });

        // Set zoom in zoom out functionality
        findViewById(R.id.button_font_plus).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                fontSizeEm += fontSizeStep;
                changedFontUpdate = true;
            }
        });

        findViewById(R.id.button_font_minus).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                fontSizeEm -= fontSizeStep;
                if(fontSizeEm < minFontSize)
                    fontSizeEm = minFontSize;

                changedFontUpdate = true;
            }
        });

        // Set donate button functionality
        findViewById(R.id.button_donate).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Uri uri = Uri.parse(getString(R.string.donate));
                Intent intent = new Intent(Intent.ACTION_VIEW, uri);
                startActivity(intent);
            }
        });

        // Set store rating functionality
        conditionallyShowAppReviewDialog();

        webview = findViewById(R.id.webview);
        //webview.setWebViewClient(new WebViewClient());
        //webview.setWebChromeClient(new WebChromeClient());

        WebSettings settings = webview.getSettings();
        settings.setJavaScriptEnabled(true); // If required
        // added on 3.9.2026
        settings.setSupportZoom(false);//true);
        settings.setBuiltInZoomControls(false);//true);
        settings.setDisplayZoomControls(false); // Hide onscreen +/- buttons
        //settings.setUseWideViewPort(true);
        //settings.setLoadWithOverviewMode(false);
        //webview.setFocusable(true);
        //webview.setFocusableInTouchMode(true);
        //webview.requestFocus(View.FOCUS_DOWN);
        // -----------------------

        webview.setVerticalScrollBarEnabled(true);
        initializeWebview();

        ((TextView)findViewById(R.id.textview_privacy)).setMovementMethod(LinkMovementMethod.getInstance());

        parseNewText("Here the recognized speech will be displayed.", getInitColor(), true);

        viewModel = new ViewModelProvider(this).get(MainViewModel.class);
        viewModel.setContext(getApplicationContext());
        viewModel.setApplication(getApplication());
        viewModel.initLifecycle(getLifecycle());
        viewModel.setMainCallback(new MainViewModel.MainCallback() {
            @Override
            public void onReceiveMessage(int msg) {
                Log.i("UHO_Transcribe", "Message: " + msg);
                switch (msg) {
                    case MSG_LOADING:
                        parseNewText("Loading, please wait...", getInitColor(), true);
                        break;

                    case MSG_STARTED:
                        parseNewText("Speech recognition active.", getInitColor(), true);
                        break;

                    case MSG_STARTERROR:
                        parseNewText("Error starting.", getErrorColor(), true);
                        break;
                }
            }

            @Override
            public void onNewText(String text) {
                long systemTimeCurr = System.nanoTime();
                double elapsedSeconds = (double)(systemTimeCurr - systemTimePrev)/1.0e9;
                systemTimePrev = systemTimeCurr;

                onNewTextHandler(text, (float)elapsedSeconds);
                //incFullText = text;
                parseNewText(incFullText, getSpeechColor(), changedFontUpdate);
                changedFontUpdate = false;
                Log.i("UHO_Transcribe", "parseNewText");
            }
        });

        showEnableMicrophone(false);
    }

    int newLen = 0;
    int prevLen = 0;
    float prevLettersPerSec = 5.0f;
    float endIndex = 0.0f;
    long systemTimePrev = System.nanoTime();
    String incFullText;

    void onNewTextHandler(String fullText, float timeElapsedSeconds) {
        int minLettersPerSecond = 5;	// config
        int maxLettersPerSecond = 100;	// config
        float normalSecondsPerUpdate = viewModel.nativeGetTimeASR(); //0.6f;	// config
        float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
        float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

        //Log.i("UHO_Transcribe", String.format("normalSecondsPerUpdate = %.3f\n", normalSecondsPerUpdate));
        //Log.i("UHO_Transcribe", String.format("timeElapsedSeconds = %.4f\n", timeElapsedSeconds));
        newLen = fullText.length();
        if (newLen < prevLen) {
            prevLen = newLen;
            endIndex = Math.clamp(endIndex, 0.0f, (float)newLen);
        }

        float lettersPerSec;
        if (newLen == prevLen) {
            lettersPerSec = prevLettersPerSec; //maxLettersPerSecond;
        }
        else {
            float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
            //secondsPerLetter = Math.min(maxSecondsPerLetter, secondsPerLetter);	// in [sec]
            secondsPerLetter = Math.clamp(secondsPerLetter, minSecondsPerLetter, maxSecondsPerLetter);	// in [sec]
            lettersPerSec = 1.0f / secondsPerLetter;
            Log.i("UHO_Transcribe", String.format("lettersPerSec = %.3f\n", lettersPerSec));
            prevLettersPerSec = lettersPerSec;
        }

        //int letterIncrement = (int)ceil(timeElapsedSeconds * lettersPerSec);
        float letterIncrement = timeElapsedSeconds * lettersPerSec;
        Log.i("UHO_Transcribe", "letterIncrement = " + letterIncrement);

        endIndex += letterIncrement;


        // additional code to account for screen orientation changes (prevents incremental display of old text)
        if(prevLen == 0) {
            prevLen = newLen;
            endIndex = newLen;
        }
        
        if (endIndex >= (float)prevLen) {
            prevLen = newLen;
        }
        Log.i("UHO_Transcribe", "endIndex = " + endIndex);
        Log.i("UHO_Transcribe", "prevLen=" + prevLen + "  newLen=" + newLen);
        
        endIndex = Math.clamp(endIndex, 0.0f, (float)newLen);

        int tmp = (int)endIndex;
        incFullText = fullText.substring(0, tmp);
    }

    /*void onNewTextHandler(String fullText, float timeElapsedSeconds) {
        int minLettersPerSecond = 5;	// config
        int maxLettersPerSecond = 100;	// config
        float normalSecondsPerUpdate = viewModel.nativeGetTimeASR(); //0.6f;	// config
        float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
        float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

        Log.i("UHO_Transcribe", String.format("normalSecondsPerUpdate = %.3f\n", normalSecondsPerUpdate));
        Log.i("UHO_Transcribe", String.format("timeElapsedSeconds = %.4f\n", timeElapsedSeconds));

        newLen = fullText.length();
        if (newLen < prevLen) {
            prevLen = newLen;
            endIndex = (float)newLen;
        }

        float lettersPerSec;
        if (newLen == prevLen) {
            Log.i("UHO_Transcribe", String.format("newLen = prevLen\n", timeElapsedSeconds));
            lettersPerSec = maxLettersPerSecond;
        }
        else {
            float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
            //secondsPerLetter = Math.min(maxSecondsPerLetter, secondsPerLetter);	// in [sec]
            secondsPerLetter = Math.clamp(secondsPerLetter, minSecondsPerLetter, maxSecondsPerLetter);	// in [sec]
            lettersPerSec = 1.0f / secondsPerLetter;
            Log.i("UHO_Transcribe", String.format("lettersPerSec = %.3f\n", lettersPerSec));
        }

        //int letterIncrement = (int)ceil(timeElapsedSeconds * lettersPerSec);
        float letterIncrement = timeElapsedSeconds * lettersPerSec;
        //Log.i("UHO_Transcribe", "letterIncrement = " + letterIncrement);

        endIndex += letterIncrement;
        if (endIndex >= (float)newLen) {
            endIndex = (float)newLen;
            prevLen = newLen;
        }

        int tmp = Math.clamp((int)endIndex, 0, (int)fullText.length());
        incFullText = fullText.substring(0, tmp);
    }*/

    boolean darkModeEnabled = false;

    void initialChangeDarkModeState() {
        changeDarkModeState(getResources().getConfiguration());
    }

    void changeDarkModeState(Configuration config) {
        int nightModeFlags = config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
        if (nightModeFlags == Configuration.UI_MODE_NIGHT_YES) {
            darkModeEnabled = true;
        } else {
            darkModeEnabled = false;
        }
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        changeDarkModeState(newConfig);
    }

    @Override
    protected void onDestroy() {
        Log.i("UHO_Transcribe", "[TranscribeActivity] onDestroy called");
        super.onDestroy();
        viewModel.setMainCallback(null);

        if (webview != null) {  // (webview de-init suggested by 'gpt')
            // Step 1: Remove from the XML parent layout
            ViewGroup parent = (ViewGroup) webview.getParent();
            if (parent != null) {
                parent.removeView(webview);
            }

            // Step 2: Clear touch listeners & callbacks
            webview.setOnTouchListener(null);
            webview.stopLoading();
            webview.clearHistory();
            webview.removeAllViews();

            // Step 3: Destroy
            webview.destroy();
        }
    }

    void conditionallyShowAppReviewDialog() {
        int showFrequency = 5;

        SharedPreferences prefs;
        prefs = getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);

        int runCount = prefs.getInt("runCount", 0);
        runCount++;

        SharedPreferences.Editor ed = prefs.edit();
        ed.putInt("runCount", runCount);
        ed.apply();

        if(runCount == 10) {
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(TranscribeActivity.this);
            dlgAlert.setMessage(
                    HtmlCompat.fromHtml( //Html.fromHtml(
                            "Hello! :) It looks like you used application UHO Global multiple times.<br>" +
                                    "In my available free time I develop applications and devices for people with special needs.<br>" +
                                    "It would be of great help, if you could support my work with a <b>donation</b>, even the smallest amount is more than welcome!<br><br>" +
                                    "Thank you for using the app!<br>" +
                                    "Blaž Kovačič, author of application UHO Global",
                            HtmlCompat.FROM_HTML_MODE_COMPACT
                    )
            );

            dlgAlert.setTitle("Message from application author");
            dlgAlert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) { }
            });
            dlgAlert.show();
            return;
        }

        if((runCount % showFrequency) == 0) {
            Log.i("UHO_Transcribe", "Launching review dialog");
            ReviewManager manager = ReviewManagerFactory.create(this);

            Task<ReviewInfo> request = manager.requestReviewFlow();
            request.addOnCompleteListener(task -> {
                if (task.isSuccessful()) {
                    // We can get the ReviewInfo object
                    ReviewInfo reviewInfo = task.getResult();
                    Task<Void> flow = manager.launchReviewFlow(this, reviewInfo);
                    flow.addOnCompleteListener(new OnCompleteListener<Void>() {
                        @Override
                        public void onComplete(@NonNull Task<Void> task) {

                        }
                    });
                } else {
                    // There was some problem, log or handle the error code.
                    @ReviewErrorCode int reviewErrorCode = ((ReviewException) task.getException()).getErrorCode();
                }
            });
        }
    }

    /*
    String prevText = "";
    void parseNewText(String newText, String fontColor, boolean forceUpdate) {
        if(newText == null || newText.isEmpty() || (prevText.equals(newText) && !forceUpdate))
            return;
        prevText = newText;

        newText = newText.replace("*!", "!</br></br>");
        newText = newText.replace("*,", ",</br></br>");
        newText = newText.replace("*.", ".</br></br>");
        newText = newText.replace("*?", "?</br></br>");
        newText = newText.replace("*", "</br></br>");

        String bodyStyle = "<body style='background-color:" + getBackgroundColor() + "'>";

        String htmlContent = "<html><head><meta charset='utf-8'/></head>" +
                bodyStyle +
                "<p id='content' style='font-size: " + fontSizeEm.toString() + "em; color: " + fontColor + "'>" + newText + "</p></body></html>";
        String js1 =
                "function emToPixelsQuick(emValue) {\n" +
                    "return emValue * parseFloat(getComputedStyle(document.body).fontSize);\n" +
                "}\n" +
                "\n" +
                "function updateGUI() {\n" +
                    "if(window.getSelection().toString().length === 0) {\n" +
                        "var pos = window.innerHeight + window.scrollY;\n" +
                        "var max = document.body.scrollHeight;\n" +
                        "document.open();\n" +
                        "document.write(\"" + htmlContent + "\");\n" +
                        "document.close();\n" +
                        "\n" +
                        "var lineHeight = 1.2 * emToPixelsQuick(" + fontSizeEm.toString() + ");\n" +
                        "if(pos >= (max - " + numLinesBottomScroll.toString() + "*lineHeight))\n" +
                            "window.scrollTo({ top: max, behavior: 'smooth' });\n" +
                    "}\n" +
                "}\n" +
                "if(window.getSelection().toString().length === 0) {\n" +
                    "updateGUI();\n" +
                "} else {\n" +
                    "document.addEventListener(\"selectionchange\", function myOnSel() { updateGUI(); document.removeEventListener(\"selectionchange\", myOnSel);\n });\n" +
                "}\n";
        webview.post(new Runnable() {
            @Override
            public void run() {
                webview.evaluateJavascript(js1, null);
            }
        });
    }*/

    void initializeWebview() {
        String bodyStyle = "<body style='background-color:" + getBackgroundColor() + "'>";
        String htmlContent =
                "<html><head>" +
                "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>" +
                "<meta charset='utf-8'/></head>" +
                bodyStyle +
                "<p id='content'></p></body></html>";

        String js = "document.open();\n" +
                "document.write(\"" + htmlContent + "\");\n" +
                "document.close();\n";

        webview.setOnTouchListener(new View.OnTouchListener() {
            float dhPrev = -1.0f;
            boolean multiTouch = false;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                int action = event.getActionMasked();
                int pointerCount = event.getPointerCount();

                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_MOVE:
                        isTouchActive = true;
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        isTouchActive = false;
                        break;
                }

                if(pointerCount == 2) {
                    float dx = event.getX(0) - event.getX(1);
                    float dy = event.getY(0) - event.getY(1);
                    float dh = (float)Math.hypot(dx, dy);

                    if(action == MotionEvent.ACTION_MOVE) {
                        if(dhPrev > 0.0f) {
                            float scaleFactor = dh / dhPrev;
                            precisionOnScale(scaleFactor);
                        }
                    }
                    dhPrev = dh;
                    //scaleGestureDetector.onTouchEvent(motionEvent);
                    //onTouchPinch(motionEvent);
                    //Log.i("UHO_Transcribe", String.format("pinchFactor = %.2f", pinchFactor));
                }


                if(action == MotionEvent.ACTION_POINTER_DOWN && pointerCount >= 2) {
                    if(!multiTouch) {
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

                if(action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP)
                    multiTouch = false;

                if(multiTouch || pointerCount >= 2)
                    return true;
                else
                    return false; // Return false to allow normal touch/scroll events to proceed
            }

            float scaleAtUpdateFont = 1.0f;
            float increaseScaleUpdate = 1.15f;//1.2f;    // bigger value means more textview zoom needed to change font
            float decreaseScaleUpdate = 0.85f;//0.8f;

            public void precisionOnScale(float scale) {
                Log.i("UHO_Captions", String.format("Scale factor %.5f detected", scale));

                scaleAtUpdateFont = scaleAtUpdateFont * scale;
                if(scaleAtUpdateFont >= increaseScaleUpdate) {
                    //updateFont(true);
                    fontSizeEm += fontSizeStep;
                    changedFontUpdate = true;

                    scaleAtUpdateFont = 1.0f;
                } else if(scaleAtUpdateFont <= decreaseScaleUpdate){
                    fontSizeEm -= fontSizeStep;
                    if(fontSizeEm < minFontSize)
                        fontSizeEm = minFontSize;

                    changedFontUpdate = true;

                    scaleAtUpdateFont = 1.0f;
                }
            }
        });

        webview.post(new Runnable() {
            @Override
            public void run() {
                webview.evaluateJavascript(js, null);
            }
        });
    }

    String prevText = "";
    String prevColor = "black";
    void parseNewText(String newText, String fontColor, boolean forceUpdate) {
        /*if(newText == null || newText.isEmpty() || (prevText.equals(newText) && !forceUpdate))
            return;
        prevText = newText;*/
        if(prevText.equals(newText) && !forceUpdate)
            return;
        if(newText == null || newText.isEmpty()) {
            newText = prevText;
            fontColor = prevColor;
        }
        prevText = newText;
        prevColor = fontColor;

        newText = newText.replace("*!", "!\n\n");
        newText = newText.replace("*,", ",\n\n");
        newText = newText.replace("*.", ".\n\n");
        newText = newText.replace("*?", "?\n\n");
        newText = newText.replace("*¿", "¿\n\n");
        newText = newText.replace("*।", "।\n\n");
        newText = newText.replace("*", "\n\n");

        boolean updateText = forceUpdate || !isTouchActive;

        String tmp =
                "document.body.style.backgroundColor = '"+ getBackgroundColor() + "';" +
                "var p = document.getElementById('content');" +
                "p.style.whiteSpace = 'pre-wrap';" +
                "p.style.color = '" + fontColor + "';" +
                "p.style.fontSize = '" + fontSizeEm.toString() + "em';" +
                "p.innerText = " + JSONObject.quote(newText) + ";";

        String js1 =
                "function emToPixelsQuick(emValue) {\n" +
                        "return emValue * parseFloat(getComputedStyle(document.body).fontSize);\n" +
                "}\n" +
                "\n" +
                "function updateGUI() {\n" +
                "if(window.getSelection().toString().length === 0 && " + (updateText?"1":"0") + ") {\n" +
                "var pos = window.innerHeight + window.scrollY;\n" +
                "var max = document.body.scrollHeight;\n" +
                tmp +
                "var lineHeight = 1.2 * emToPixelsQuick(" + fontSizeEm.toString() + ");\n" +
                "if(pos >= (max - " + numLinesBottomScroll.toString() + "*lineHeight))\n" +
                "window.scrollTo({ top: max, behavior: 'smooth' });\n" +
                "}\n" +
                "}\n" +
                "if(window.getSelection().toString().length === 0 && " + (updateText?"1":"0") + ") {\n" +
                "updateGUI();\n" +
                "} else {\n" +
                "document.addEventListener(\"selectionchange\", function myOnSel() { updateGUI(); document.removeEventListener(\"selectionchange\", myOnSel);\n });\n" +
                "}\n";
        webview.post(new Runnable() {
            @Override
            public void run() {
                webview.evaluateJavascript(js1, null);
            }
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        // this callback gets called after requesting microphone permission
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        Log.i("UHO_Transcribe", String.format("Inside onRequestPermissionsResult with code %d", requestCode));

        if(grantResults[0] == PackageManager.PERMISSION_GRANTED)
            runSpeechRecognition();
        else {
            boolean shouldShow = ActivityCompat.shouldShowRequestPermissionRationale(TranscribeActivity.this, Manifest.permission.RECORD_AUDIO);
            if(!shouldShow)
                showEnableMicrophone(true);
            else {
                showEnableMicrophone(false);
            }
        }
    }

    boolean hasMicrophonePermission() {
        if(ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            Log.i("UHO_Transcribe", "Microphone permission already granted");
            return true;
        }

        return false;
    }

    ActivityResultLauncher<Intent> settingsLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            new ActivityResultCallback<ActivityResult>() {
                @Override
                public void onActivityResult(ActivityResult o) {
                    showEnableMicrophone(false);
                }
            }
    );

    void openSettings() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        Uri uri = Uri.fromParts("package", getPackageName(), null);
        intent.setData(uri);
        settingsLauncher.launch(intent);
    }

    void showEnableMicrophone(boolean showSettings) {
        if(hasMicrophonePermission()) {
            runSpeechRecognition();
            return;
        }

        String[] permissions = { Manifest.permission.RECORD_AUDIO };
        boolean shouldShow = ActivityCompat.shouldShowRequestPermissionRationale(TranscribeActivity.this, Manifest.permission.RECORD_AUDIO);
        if(!shouldShow && !showSettings) {
            ActivityCompat.requestPermissions(TranscribeActivity.this, permissions, REQUEST_PERMISSION_MICROPHONE);
            return;
        }

        AlertDialog.Builder dlgAlert = new AlertDialog.Builder(this);
        dlgAlert.setMessage("Application UHO requires microphone access to work.");
        dlgAlert.setTitle("UHO - permission (microphone)");
        dlgAlert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                Log.i("UHO_Transcribe", "Requesting record audio permission");
                if(showSettings)
                    openSettings();
                else
                    ActivityCompat.requestPermissions(TranscribeActivity.this, permissions, REQUEST_PERMISSION_MICROPHONE);
            }
        });

        dlgAlert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                finish();
            }
        });
        dlgAlert.setCancelable(true);
        dlgAlert.create().show();
    }

    void runSpeechRecognition() {
        viewModel.start();
    }
}
