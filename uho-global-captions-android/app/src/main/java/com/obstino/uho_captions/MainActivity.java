// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_captions;

import static android.text.Html.FROM_HTML_MODE_COMPACT;
import static android.text.Html.FROM_HTML_MODE_LEGACY;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.ListPopupWindow;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.text.HtmlCompat;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.work.Data;
import androidx.work.ExistingWorkPolicy;
import androidx.work.OneTimeWorkRequest;
import androidx.work.Operation;
import androidx.work.OutOfQuotaPolicy;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.appcompat.app.AlertDialog;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.Html;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;

import android.media.projection.MediaProjectionManager;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.Filter;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.tasks.OnCanceledListener;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.Task;
import com.google.android.material.snackbar.Snackbar;
import com.google.android.material.textfield.MaterialAutoCompleteTextView;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;
import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;
import com.google.android.play.core.review.ReviewException;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;
import com.google.android.play.core.review.model.ReviewErrorCode;
import com.google.common.util.concurrent.ListenableFuture;
import com.obstino.uho_captions.databinding.ActivityMainBinding;
import com.obstino.uho_captions.Downloader;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

public class MainActivity extends AppCompatActivity {

    static enum TokenizerType {
        none,
        RoBERTa,
        BERT,
    };

    static class LanguageInfo {
        String dropdownText;
        String assetName;
        String asrModelPath;
        String asrVocabPath;
        String pncModelPath;
        String pncVocabPath;
        boolean installTime;
        boolean isConformer;
        float timeBias;
        boolean unspacedText;   // refers to model type, e.g. if its char-based model, such as the ja_en model
        boolean punctuate;
        TokenizerType pncTokType;
        char[] pncSymbols;
        boolean pncUseCapitalLetters;
        boolean suppressTokens;
        boolean enableWordWrap;    // normally set true (e.g. English model), otherwise breaks line "mid-word" when printing >=2 lines of captions (ok for languages with no space symbols such as Chinese)

        LanguageInfo(
                String dropdownText,
                String assetName,
                String asrModelPath,
                String asrVocabPath,
                String pncModelPath,
                String pncVocabPath,
                boolean installTime,
                boolean isConformer,
                float timeBias,
                boolean unspacedText,
                boolean punctuate,
                TokenizerType pncTokType,
                char[] pncSymbols,
                boolean pncUseCapitalLetters,
                boolean suppressTokens,
                boolean enableWordWrap
        )
        {
            this.dropdownText = dropdownText;
            this.assetName = assetName;
            this.asrModelPath = asrModelPath;
            this.asrVocabPath = asrVocabPath;
            this.pncModelPath = pncModelPath;
            this.pncVocabPath = pncVocabPath;
            this.installTime = installTime;
            this.isConformer = isConformer;
            this.timeBias = timeBias;
            this.unspacedText = unspacedText;
            this.punctuate = punctuate;
            this.pncTokType = pncTokType;
            this.pncSymbols = pncSymbols;
            this.pncUseCapitalLetters = pncUseCapitalLetters;
            this.suppressTokens = suppressTokens;
            this.enableWordWrap = enableWordWrap;
        }
    };

    static List<LanguageInfo> langInfo = List.of(
            new LanguageInfo("Arabic (ar-AR)", "assets_ar", "ASRMODEL_Conformer-CTC-L_spe128_ar-AR_3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe128_ar-AR_3.0.txt", "", "", false, true, 0.024f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Belarusian (be-BY)", "assets_by", "ASRMODEL_stt_by_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_by_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Brazilian Portuguese (pt-BR)", "assets_pt", "ASRMODEL_Conformer-CTC-L_bpe1024_pt-BR_1.2.int8.onnx", "ASRVOCAB_Conformer-CTC-L_bpe1024_pt-BR_1.2.txt", "PNCMODEL_bert-base_pt-BR_Riva-PnC-SET-1.0_quantized_weights.onnx", "PNCVOCAB_pt.txt", false, true, 0.024f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            new LanguageInfo("Catalan (ca)", "assets_ca", "ASRMODEL_stt_ca_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_ca_conformer_ctc_large.txt", "", "", false, true, 0.07f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Chinese (zh-CN)-English (en-US)", "assets_zh_en", "ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char[] {}, false, false, false),
            new LanguageInfo("Croatian (hr)", "assets_hr", "ASRMODEL_stt_hr_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_hr_conformer_ctc_large.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Dutch (nl)", "assets_nl", "ASRMODEL_Conformer-CTC-L_spe256_nl-NL_1.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe256_nl-NL_1.0.txt", "PNCMODEL_bert-base-nl-NL_1.0_quantized_weights.onnx", "PNCVOCAB_nl.txt", false, true, 0.100f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            new LanguageInfo("English (en-US)", "assets_en", "ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt", "PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx", "PNCVOCAB_en.txt", true, true, 0f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            new LanguageInfo("Esperanto (eo)", "assets_eo", "ASRMODEL_stt_eo_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_eo_conformer_ctc_large.txt", "", "", false, true, 0.035f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("French (fr)", "assets_fr", "ASRMODEL_stt_fr_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_fr_conformer_ctc_large.txt", "PNCMODEL_bert-base_fr_fr_Riva-PnC-SET-2.0_quantized_weights.onnx", "PNCVOCAB_fr.txt", false, true, 0.1f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '-', '.', '?'}, true, false, true),
            new LanguageInfo("German (de)", "assets_de", "ASRMODEL_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.txt", "PNCMODEL_punct_de_quantized_weights.onnx", "PNCVOCAB_de.txt", false, true, 0.0f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            //new LanguageInfo("Hindi (hi)", "assets_hi", "ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt", "PNCMODEL_bert-base_hi-IN_Riva-PnC-SET-1.0_opt_quantized_weights.onnx", "PNCVOCAB_hi.txt", true, true, 0f, false, true, TokenizerType.BERT, new char[] {'\0', '!', ',', '?', '।'}, false, false),
            new LanguageInfo("Hindi (hi)", "assets_hi", "ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char [] {}, false, false, true),
            new LanguageInfo("Italian (it)", "assets_it", "ASRMODEL_Conformer-CTC-L_spe1024_it-IT_2.1.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe1024_it-IT_2.1.txt", "PNCMODEL_bert-base_it-IT_Riva-PnC-SET-1.1_quantized_weights.onnx", "PNCVOCAB_it.txt", false, true, 0.100f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            new LanguageInfo("Japanese (ja-JP)-English (en-JP)", "assets_ja_en", "ASRMODEL_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.txt", "", "", false, true, -0.016f, true, false, TokenizerType.none, new char[] {}, false, false, false),
            new LanguageInfo("Kinyarwanda (rw)", "assets_rw", "ASRMODEL_stt_rw_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_rw_conformer_ctc_large.txt", "", "", false, true, 0.14f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Korean (ko-KR)", "assets_ko", "ASRMODEL_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.txt", "PNCMODEL_bert-base_ko-KR_Riva-PnC-SET-1.1_quantized_weights.onnx", "PNCVOCAB_ko.txt", false, true, -0.024f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, false, false, true),
            new LanguageInfo("Marathi (mr)", "assets_mr", "ASRMODEL_stt_mr_conformer_ctc_medium.int8.onnx", "ASRVOCAB_stt_mr_conformer_ctc_medium.txt", "", "", false, true, -0.135f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Persian (fa)", "assets_fa", "ASRMODEL_stt_fa_fastconformer_hybrid_large.int8.onnx", "ASRVOCAB_stt_fa_fastconformer_hybrid_large.txt", "", "", false, false, -0.056f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Polish (pl)", "assets_pl", "ASRMODEL_stt_pl_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_pl_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0.237f, false, false, TokenizerType.none, new char[] {}, false, false, true),
            new LanguageInfo("Russian (ru)", "assets_ru", "ASRMODEL_stt_ru_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_ru_conformer_ctc_large.txt", "PNCMODEL_bert-base_ru-RU_Riva-PnC-SET-1.0_quantized_weights.onnx", "PNCVOCAB_ru.txt", false, true, -0.016f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false, true),
            new LanguageInfo("Slovenian (sl-SI)", "assets_sl", "ASRMODEL_sl.int8.onnx", "ASRVOCAB_sl.txt", "PNCMODEL_punct_sl_quantized_weights.onnx", "PNCVOCAB_sl.txt", false, true, 0f, false, true, TokenizerType.RoBERTa, new char[] {'\0', '!', ',', '.', '?'}, true, true, true),
            new LanguageInfo("Spanish (es-US)-English (en-US)", "assets_es_en", "ASRMODEL_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.txt", "PNCMODEL_bert-base_es-US_Riva-PnC-SET-2.0_quantized_weights.onnx", "PNCVOCAB_es.txt", false, true, -0.016f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?', '¿'}, true, false, true),
            new LanguageInfo("Ukrainian (ua)", "assets_ua", "ASRMODEL_stt_ua_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_ua_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0f, false, false, TokenizerType.none, new char[] {}, false, false, true)
    );

    static String uniqueWorkName = "dlworkcaptions";
    static String uniqueWorkTag = "dltagcaptions";

    SharedPreferences prefs;
    static String PREFS_NAME = "PREFS_UHO_Captions";
    static String DEFAULT_CHOSEN_ASSET = "assets_en";

    private Handler mHandler = new Handler();

    boolean shouldExitActivity = false;

    static ActivityResult mediaProjectionResult;

    private ActivityMainBinding binding;
    final int REQUEST_PERMISSION_MICROPHONE = 100;
    final int REQUEST_PERMISSION_PROJECTION = 200;
    final int REQUEST_PERMISSION_ACCESSIBILITY = 300;
    final int REQUEST_PERMISSION_NOTIFICATIONS = 400;

    static final int REQUEST_CODE_SETTINGS = 400;
    static final int RESULT_EXIT = RESULT_FIRST_USER;

    static final String BROADCAST_EVENT_NAME = "MainActivityBroadcast";
    static final int MSG_STARTSTOP = 0;
    static final int MSG_GETSTARTSTATE = 1;
    static final int MSG_STOPPED = 2;
    static final int MSG_STARTED = 3;
    static final int MSG_STARTERROR = 4;
    static final int MSG_WAITSTART = 5;
    static final int MSG_CAN_OPENSETTINGS = 6;
    static final int MSG_CANT_OPENSETTINGS = 7;
    static final int MSG_CAN_INITSTART = 8;
    static final int MSG_CANT_INITSTART = 9;   // not in use
    static final int MSG_CAN_INITSTOP = 10;
    static final int MSG_FINISHED = 11;
    static final int MSG_FORCESTOP = 12;    // completely shutdown the accessibility service
    static final int MSG_CELLULAR_DIALOG = 13;

    final String stringStart = "START";
    final String stringStop = "STOP";

    boolean requestingAllPermissions = false;

    MediaProjectionManager mediaProjectionManager;

    Button buttonStart;
    Button buttonSettings;
    TextInputLayout textInputLayout;
    //TextInputEditText textInputEditText;
    MaterialAutoCompleteTextView autoCompleteLang;

    TextView textViewDownloading;
    ProgressBar progressBarDownload;
    Button buttonCancelDownload;
    Button buttonDeleteAsset;

    AlertDialog downloadAlertDialog;

    MyBroadcastReceiver myBroadcastReceiver = new MyBroadcastReceiver();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i("UHO_Captions", "onCreate");

        if (!isTaskRoot()) {    // (source: https://stackoverflow.com/a/16447508)
            // Android launched another instance of the root activity into an existing task
            //  so just quietly finish and go away, dropping the user back into the activity
            //  at the top of the stack (ie: the last state of this task)
            finish();
            return;
        }

        prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // get relevant view objects
        textViewDownloading = findViewById(R.id.textview_downloading);
        progressBarDownload = findViewById(R.id.progress_download);
        buttonCancelDownload = findViewById(R.id.button_cancel_download);
        buttonDeleteAsset = findViewById(R.id.button_delete_asset);
        textInputLayout = findViewById(R.id.textinput_layout);
        //textInputEditText = findViewById(R.id.textinput_edittext);
        autoCompleteLang = findViewById(R.id.autocomplete_lang);
        buttonStart = findViewById(R.id.button_start);
        buttonSettings = findViewById(R.id.button_settings);

        conditionallyShowDeleteAsset(); // added this back on 22.9.2026

        createDownloadObserver();

        // set language selection
        //ArrayAdapter<String> arr;
        ArrayList<String> languages = new ArrayList<>();
        for(LanguageInfo info: langInfo) {
            languages.add(info.dropdownText);
        }

        //arr = new ArrayAdapter<String>(this, R.layout.list_item, R.id.mycustomtextview, languages);   // old code
        //autoCompleteLang.setAdapter(arr);                                                             // old code
        //autoCompleteLang.setSimpleItems(languages.toArray(new String[0]));  // commented on 19.8.2026

        CustomArrayAdapter<String> adapter = new CustomArrayAdapter<>(
                this,
                android.R.layout.simple_list_item_1,
                languages
        );

        /*ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, languages);
        ListPopupWindow popup = new ListPopupWindow(this);
        popup.setAnchorView(textInputLayout);
        popup.setAdapter(adapter);
        popup.setModal(true);
        popup.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                popup.dismiss();
                conditionallyDownloadAssets(position);
                textInputEditText.setText(languages.get(position));
                textInputEditText.setVisibility(View.GONE);
                textInputEditText.setVisibility(View.VISIBLE);
            }
        });
        setSelected();

        View.OnClickListener openPopupListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!popup.isShowing()) {
                    popup.show();
                    ListView listView = popup.getListView();
                    if (listView != null) {
                        listView.setScrollbarFadingEnabled(false);

                        int sizePx = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 12, getResources().getDisplayMetrics());
                        listView.setScrollBarSize(sizePx);
                    }
                }
            }
        };

        textInputEditText.setOnClickListener(openPopupListener);
        textInputLayout.setEndIconOnClickListener(openPopupListener);*/

        // commented on 23.9.2026 because AutoCompleteTextview didn't work well for dropdown (text didn't wrap and small scrollbar appeared)
        autoCompleteLang.setKeyListener(null);
        autoCompleteLang.setAdapter(adapter);
        autoCompleteLang.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
                conditionallyDownloadAssets(position);
                autoCompleteLang.setText(languages.get(position));
                autoCompleteLang.setVisibility(View.GONE);
                autoCompleteLang.setVisibility(View.VISIBLE);

            }
        });
        setSelected();

        ((TextView)findViewById(R.id.textview_privacy)).setMovementMethod(LinkMovementMethod.getInstance());

        buttonStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onStartButtonPressed();
            }
        });

        /*PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (!pm.isIgnoringBatteryOptimizations(getPackageName())) {
            Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
            intent.setData(Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        }*/

        // "SETTINGS" button logic
        buttonSettings.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent activityIntent = new Intent(MainActivity.this, SettingsActivity.class);
                startActivityForResult(activityIntent, REQUEST_CODE_SETTINGS);
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

        buttonCancelDownload.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //cancel download of asset
                WorkManager workManager = WorkManager.getInstance(getApplicationContext());
                Operation op  = workManager.cancelUniqueWork(uniqueWorkName);
                op.getResult().addListener(new Runnable() {
                    @Override
                    public void run() {
                        showDownloading(false);
                        setSelectionNotDownloaded();
                    }
                }, ContextCompat.getMainExecutor(MainActivity.this));
            }
        });

        buttonDeleteAsset.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AlertDialog alertDialog = new AlertDialog.Builder(MainActivity.this).create();
                alertDialog.setTitle("Warning");
                alertDialog.setMessage("Are you sure you want to remove language files for \"" + langInfo.get(getSelected()).dropdownText + "\"?");
                alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, "Yes",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                deleteSelectedAsset();
                                setSelectionNotDownloaded();
                                dialog.dismiss();
                            }
                        });
                alertDialog.setButton(AlertDialog.BUTTON_NEGATIVE, "No",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                            }
                        });
                alertDialog.show();
            }
        });

        notificationPermissionDialog();

        handleIntent(getIntent());
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i("UHO_Captions", "onPause");
    }

    @Override
    protected void onResume() { // called after onCreate and also whenever activity resumed
        super.onResume();

        Log.i("UHO_Captions", "onResume");

        isDownloadActive(new DownloadInfoHandler() {
            @Override
            public void callback(ActiveType type) {
                boolean cellularDialogRequired = prefs.getBoolean("assetConfirmationRequired", false);
                if(type.equals(ActiveType.active) && cellularDialogRequired)
                    showCellularDialog();
            }
        });

        if(shouldExitActivity) {
            shouldExitActivity = false;
            finishAndRemoveTask();  //finish();

            Log.i("UHO_Captions", "onActivityResult: calling workManager.cancelUniqueWork");
            WorkManager workManager = WorkManager.getInstance(getApplicationContext());
            Operation op = workManager.cancelUniqueWork(uniqueWorkName);
            op.getResult().addListener(new Runnable() {
                @Override
                public void run() {
                    Log.i("UHO_Captions", "cancelUniqueWork returned, calling System.exit(0) soon");
                    mHandler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            System.exit(0);
                        }
                    }, 1000);
                }
            }, ContextCompat.getMainExecutor(MainActivity.this));
            return;
        }

        if(!requestingAllPermissions) {
            Log.i("UHO_Captions", "[onResume] requestingAllPermissions is false");
            // code which basically updates Start/Stop button text
            if (!isAccessibilityServiceRunning()) {
                Log.i("UHO_Captions", "[onResume] calling praseMessage(MSG_STOPPED)");
                myBroadcastReceiver.parseMessage(MSG_STOPPED);
            }
            else {
                Log.i("UHO_Captions", "[onResume] calling sendBroadcastMessage");
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        sendBroadcastMessage(MainActivity.MSG_GETSTARTSTATE);
                    }
                }, 800);
            }
        } else {
            Log.i("UHO_Captions", "[onResume] requestingAllPermissions is true");
        }
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        //conditionallyShowDeleteAsset();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleIntent(intent);
    }

    void initDisableWidgets() {
        Log.i("UHO_Captions", "[initDisableWidgets] disabling");
        // disable dropdown
        disableDropdown();
        // set initial start button text, disable it, and set it to visible
        buttonStart.setText(stringStart);
        buttonStart.setEnabled(false);
        buttonStart.setVisibility(View.VISIBLE);
        // hide download-related widgets
        textViewDownloading.setVisibility(View.GONE);
        progressBarDownload.setVisibility(View.GONE);
        buttonCancelDownload.setVisibility(View.GONE);
        buttonDeleteAsset.setVisibility(View.GONE);
    }

    void disableDropdown() {
        autoCompleteLang.setEnabled(false);
        //textInputEditText.setEnabled(false);
        textInputLayout.setEnabled(false);
    }

    void enableDropdown() {
        autoCompleteLang.setEnabled(true);
        //textInputEditText.setEnabled(true);
        textInputLayout.setEnabled(true);
    }

    void setSelectionNotDownloaded() {
        String newAssetName = DEFAULT_CHOSEN_ASSET;

        SharedPreferences.Editor ed = prefs.edit();
        ed.putString("assetName", newAssetName);
        Log.i("UHO_Captions", "inside setSelectionNotDownloaded. Putting " + newAssetName + " into shared prefs");
        ed.apply();

        setSelected();
    }

    // this will create a download observer which will trigger onChanged - this functions well even before work is enqueued/started
    void createDownloadObserver() {
        WorkManager workManager = WorkManager.getInstance(getApplicationContext());
        LiveData<List<WorkInfo>> listLiveData = workManager.getWorkInfosForUniqueWorkLiveData(uniqueWorkName);

        listLiveData.observe(this, new Observer<List<WorkInfo>>() {
            @Override
            public void onChanged(List<WorkInfo> workInfos) {
                if (workInfos == null || workInfos.isEmpty())
                    return;
                WorkInfo workInfo = workInfos.get(0);
                switch (workInfo.getState()) {
                    case ENQUEUED:
                        Log.i("UHO_Captions", "[listLiveData:onChanged] ENQUEUED");
                        showDownloading(true);
                        updateProgressBar(0);
                        textViewDownloading.setText("Downloading language (" + 0 + "%)");
                        break;
                    case RUNNING:
                        showDownloading(true);
                        Data progress = workInfo.getProgress();
                        int value = progress.getInt(Downloader.PROGRESS, 0);
                        Log.i("UHO_Captions", "download percentage = " + value);
                        updateProgressBar(value);
                        textViewDownloading.setText("Downloading language (" + value + "%)");
                        break;
                    case FAILED:
                    case BLOCKED:
                    case CANCELLED:
                        if(App.downloaderJustFinished) {
                            App.downloaderJustFinished = false;
                            Log.i("UHO_Captions", "download error: " + workInfo.getState().toString());
                            showDownloading(false);
                            setSelectionNotDownloaded();
                            Toast.makeText(getApplicationContext(), "Error downloading: " + workInfo.getState().toString(), Toast.LENGTH_LONG).show();
                        }
                        break;
                    case SUCCEEDED:
                        //if(prefs.getBoolean("DownloadJustFinished", false)) {
                        if(App.downloaderJustFinished) {
                            App.downloaderJustFinished = false;
                            //SharedPreferences.Editor ed = prefs.edit();
                            //ed.putBoolean("DownloadJustFinished", false);
                            //ed.apply();
                            Log.i("UHO_Captions", "download just finished and successful");
                            showDownloading(false);
                            Toast.makeText(getApplicationContext(), "Download finished, press START to begin", Toast.LENGTH_SHORT).show();
                            conditionallyShowDeleteAsset();
                            //}
                        }
                        break;
                }
            }
        });
    }

    void updateProgressBar(int percent) {
        progressBarDownload.setProgress(percent);
    }

    void conditionallyDownloadAssets(int dropdownPos) {
        Log.i("UHO_Captions", "selected language position = " + dropdownPos);
        LanguageInfo li = langInfo.get(dropdownPos);

        if(isAssetPackInstalled(li.assetName)) {
            SharedPreferences.Editor ed = prefs.edit();
            ed.putString("assetName", li.assetName);
            Log.i("UHO_Captions", "inside onItemClick. Putting " + li.assetName + " into shared prefs");
            ed.apply();

            conditionallyShowDeleteAsset();
            return;
        }

        if(downloadAlertDialog == null || !downloadAlertDialog.isShowing())
            downloadAlertDialog = new AlertDialog.Builder(MainActivity.this).create();

        downloadAlertDialog.setTitle("Download request");
        downloadAlertDialog.setMessage("Language not installed.\nPress OK to download language speech recognition files for \"" + li.dropdownText + "\"");
        downloadAlertDialog.setButton(AlertDialog.BUTTON_POSITIVE, "OK",
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        SharedPreferences.Editor ed = prefs.edit();
                        ed.putString("assetName", li.assetName);
                        Log.i("UHO_Captions", "inside onItemClick. Putting " + li.assetName + " into shared prefs");
                        ed.apply();

                        createDownloadWorker(li.assetName);

                        dialog.dismiss();
                    }
                });
        downloadAlertDialog.setButton(AlertDialog.BUTTON_NEGATIVE, "Cancel",
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        SharedPreferences.Editor ed = prefs.edit();
                        ed.putString("assetName", DEFAULT_CHOSEN_ASSET);
                        ed.apply();
                        conditionallyShowDeleteAsset(); // show delete asset button when appropriate
                        setSelected();  // sets chosen asset in dropdown
                        dialog.dismiss();
                    }
                });
        downloadAlertDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                SharedPreferences.Editor ed = prefs.edit();
                ed.putString("assetName", DEFAULT_CHOSEN_ASSET);
                ed.apply();
                conditionallyShowDeleteAsset(); // show delete asset button when appropriate
                setSelected();  // sets chosen asset in dropdown
                dialog.dismiss();
            }
        });
        downloadAlertDialog.show();
    }

    void deleteSetEnabled(boolean enableViews) {
        conditionallyShowDeleteAsset();

        if(enableViews) {
            enableDropdown();
            // todo show hide buttonStart
            buttonStart.setEnabled(true);
        } else {
            Log.i("UHO_Captions", "deleting, setting disabled");
            disableDropdown();
            // todo show hide buttonStart
            //buttonStart.setEnabled(false);
        }
    }

    void deleteSelectedAsset() {
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(getApplicationContext());
        if(isAssetPackInstalled(asset) && !isAssetPackInstallTime(asset)) {
            deleteSetEnabled(false);
            Task<Void> task = assetPackManager.removePack(asset);
            task.addOnCompleteListener(new OnCompleteListener<Void>() {
                @Override
                public void onComplete(@NonNull Task<Void> task) {
                    deleteSetEnabled(true);
                }
            });

            task.addOnCanceledListener(new OnCanceledListener() {
                @Override
                public void onCanceled() {
                    deleteSetEnabled(true);
                }
            });
            task.addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(@NonNull Exception e) {
                    deleteSetEnabled(true);
                }
            });
        }
    }

    void conditionallyShowDeleteAsset() {
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        if(isAssetPackInstalled(asset) && !isAssetPackInstallTime(asset)) {
            // since we can assume it's not downloading, we can show delete button
            buttonDeleteAsset.setVisibility(View.VISIBLE);
        } else {
            buttonDeleteAsset.setVisibility(View.GONE);
        }

        // TODO buttonStart show hide? alertdialog?
        /*if(isAssetPackInstalled(asset)) {
            buttonStart.setText(BTN_START_INSTALLED);
        } else {
            buttonStart.setText(BTN__NOTINSTALLED);
        }*/
    }

    void showDownloading(boolean downloading) {
        if(downloading) {
            Log.i("UHO_Captions", "downloading, setting disabled");
            disableDropdown();
            buttonStart.setVisibility(View.GONE);
            buttonCancelDownload.setVisibility(View.VISIBLE);
            textViewDownloading.setVisibility(View.VISIBLE);
            progressBarDownload.setVisibility(View.VISIBLE);
        } else {
            Log.i("UHO_Captions", "setting enabled");
            enableDropdown();
            buttonStart.setVisibility(View.VISIBLE);
            buttonCancelDownload.setVisibility(View.GONE);
            textViewDownloading.setVisibility(View.GONE);
            progressBarDownload.setVisibility(View.GONE);
        }
    }

    void createDownloadWorker(String packName) {
        Log.i("UHO_Captions", "Creating download Worker");

        //SharedPreferences.Editor ed = prefs.edit();
        //ed.putBoolean("DownloadJustFinished", false);
        //App.flagCancelUniqueWork = false;
        //ed.apply();

        WorkManager workManager = WorkManager.getInstance(getApplicationContext());
        Data inputData = new Data.Builder()
                .putString("packname", packName)
                .build();

        OneTimeWorkRequest workRequest = new OneTimeWorkRequest.Builder(Downloader.class)
                .setInputData(inputData)
                .addTag(uniqueWorkTag)
                .setExpedited(OutOfQuotaPolicy.RUN_AS_NON_EXPEDITED_WORK_REQUEST)
                .build();

        workManager.enqueueUniqueWork(uniqueWorkName, ExistingWorkPolicy.REPLACE, workRequest);
        showDownloading(true);
    }

    int getSelected() {
        String selection = autoCompleteLang.getText().toString();
        //String selection = textInputEditText.getText().toString();

        int idxSelected = 0;
        for(int i = 0; i < langInfo.size(); i++) {
            LanguageInfo li = langInfo.get(i);
            if(li.dropdownText.equals(selection))
                idxSelected = i;
        }

        return idxSelected;
    }

    // call this function after autoCompleteLang has been populated
    void setSelected() {
        int idxSelected = 0;
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        for(int i = 0; i < langInfo.size(); i++) {
            LanguageInfo li = langInfo.get(i);
            if(li.assetName.equals(asset))
                idxSelected = i;
        }

        final int finalIndex = idxSelected;
        /*autoCompleteLang.postDelayed(new Runnable() {
            @Override
            public void run() {
                autoCompleteLang.setText(langInfo.get(finalIndex).dropdownText, false);
            }
        }, delay?300:0);*/

        autoCompleteLang.setText(langInfo.get(finalIndex).dropdownText, false);
        //textInputEditText.setText(langInfo.get(finalIndex).dropdownText);

        // TODO buttonStart show hide? alertdialog?
        //if(isAssetPackInstalled(asset)) {
        //    buttonStart.setText(BTN_START_INSTALLED);
        //} else {
        //    buttonStart.setText(BTN_START_NOTINSTALLED);
        //}
    }

    public class CustomArrayAdapter<T> extends ArrayAdapter<T> {
        private final List<T> items;

        public CustomArrayAdapter(@NonNull Context context, int resource, @NonNull List<T> objects) {
            super(context, resource, objects);
            this.items = objects;
        }

        @NonNull
        @Override
        public Filter getFilter() {
            return new Filter() {
                @Override
                protected FilterResults performFiltering(CharSequence constraint) {
                    FilterResults results = new FilterResults();
                    results.values = items;
                    results.count = items.size();
                    return results;
                }

                @Override
                protected void publishResults(CharSequence constraint, FilterResults results) {
                    notifyDataSetChanged();
                }
            };
        }
    }

    interface DownloadInfoHandler {
        enum ActiveType {
            inactive,
            active,
            enqueued
        }
        void callback(ActiveType type);
    }

    boolean isAssetPackInstallTime(String packName) {
        for(LanguageInfo li: langInfo) {
            if(li.assetName.equals(packName) && li.installTime)
                return true;
        }

        return false;
    }

    boolean isAssetPackInstalled(String packName) {
        if(isAssetPackInstallTime(packName))
            return true;

        Context applicationContext = getApplicationContext();
        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);
        AssetPackLocation location = assetPackManager.getPackLocation(packName);
        if(location == null)
            return false;

        return true;
    }

    void isDownloadActive(DownloadInfoHandler downloadInfoHandler) {
        WorkManager workManager = WorkManager.getInstance(getApplicationContext());
        ListenableFuture<List<WorkInfo>> listenableFuture = workManager.getWorkInfosByTag(uniqueWorkTag);
        listenableFuture.addListener(new Runnable() {
            @Override
            public void run() {
                List<WorkInfo> workInfos = null;
                try {
                    workInfos = listenableFuture.get();
                } catch (ExecutionException e) {
                    downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                    return;
                    //throw new RuntimeException(e);
                } catch (InterruptedException e) {
                    downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                    return;
                    //throw new RuntimeException(e);
                }

                if (workInfos == null || workInfos.isEmpty()) {
                    downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                    return;
                }
                WorkInfo workInfo = workInfos.get(0);
                switch (workInfo.getState()) {
                    case ENQUEUED:
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.enqueued);
                        break;
                    case RUNNING:
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.active);
                        //Data progress = workInfo.getProgress();
                        //int value = progress.getInt(Downloader.PROGRESS, 0);
                        //Log.i("UHO_Captions", "download percentage = " + value);
                        break;
                    case FAILED:
                    case BLOCKED:
                    case CANCELLED:
                        //Log.i("UHO_Captions", "download error");
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                        break;
                    case SUCCEEDED:
                        //Log.i("UHO_Captions", "download successful");
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                        break;
                }
            }
        }, ContextCompat.getMainExecutor(this));
    }

    void notificationPermissionDialog() {
        //if(0==0)
        //    return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                /*
                AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setTitle("Prikaz gumba Zaženi/Ustavi");
                builder.setMessage("Želite gumb Zaženi/Ustavi v oknu z obvestili za hiter dostop do aplikacije? (priporočljivo DA)");
                builder.setPositiveButton("DA", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                    }
                });
                builder.setNeutralButton("Morda kasneje", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {

                    }
                });

                // Create the AlertDialog.
                AlertDialog dialog = builder.create();*/

                Snackbar mySnackbar = Snackbar.make(
                        findViewById(R.id.main_coord_view),
                        "Do you want the Start/Stop button in notification panel?",
                        Snackbar.LENGTH_INDEFINITE);
                mySnackbar.setAction("YES", new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQUEST_PERMISSION_NOTIFICATIONS);
                    }
                });
                mySnackbar.show();
            }
        }
    }

    void handleIntent(Intent intent) {
        if(intent == null || intent.getAction() == null || requestingAllPermissions)
            return;

        if("ACTION_SHOW_AND_START".equals(intent.getAction())) {
            if(downloadAlertDialog != null && downloadAlertDialog.isShowing()) {
                Log.i("UHO_Captions", "[MainActivity:handleIntent] ACTION_SHOW_AND_START received, but alert is still showing.");
                return;
            }

            String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
            if(!isAssetPackInstalled(asset)) {
                setSelectionNotDownloaded();    // sets selection and prefs to default asset name
                return;
            }

            Log.i("UHO_Captions", "[MainActivity:handleIntent] ACTION_SHOW_AND_START. Calling onStartButtonPressed");
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    onStartButtonPressed();
                }}, 500);
        } else {
            Log.i("UHO_Captions", "[MainActivity:handleIntent] End of handleIntent");
        }
    }

    @Override
    protected void onStart() {
        super.onStart();

        Log.i("UHO_Captions", "[onStart]");

        LocalBroadcastManager.getInstance(getApplicationContext()).registerReceiver(myBroadcastReceiver,
                new IntentFilter(MainActivity.BROADCAST_EVENT_NAME));

        initDisableWidgets();
    }

    @Override
    protected void onStop() {
        super.onStop();

        // requestingAllPermissions is a variable that essentialy makes sure that during onResume that comes
        // after onPause the application will continue to block views such as the language dropdown
        // because dialogs such as permission requests are still showing
        // (if those dialogs are dismissed, the code logic makes sure requestingAllPermissions becomes false)

        // We set requestingAllPermissions to false here for the following reasons:
        // 1) If it were true before, it means permission dialog such as "Allow mic recording?" was showing
        // and now the activity with that dialog is being closed, along with the dialog.
        // 2) This means that during next execution of code inside onResume we want the code there
        // to probe the service or directly receive a message inside broadcast receiver.
        // Such a message may be MSG_STOPPED, which in turn enables dropdown and other views again, making sure
        // that user can continue interaction even after activity was closed/destroyed.

        Log.i("UHO_Captions", "[onStop] setting requestingAllPermissions = false");
        requestingAllPermissions = false;

        try {
        LocalBroadcastManager.getInstance(getApplicationContext()).unregisterReceiver(myBroadcastReceiver);
        } catch (IllegalArgumentException e) {
            Log.i("UHO_Captions", "[MainActivity] unregisterReceiver exception");
        }
    }

    void onStartButtonPressed() {
        if(!isAccessibilityServiceRunning())
            myBroadcastReceiver.parseMessage(MSG_CAN_INITSTART);
        else
            sendBroadcastMessage(MainActivity.MSG_CAN_INITSTART);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    /*
     ** Based on {@link com.android.settingslib.accessibility.AccessibilityUtils#getEnabledServicesFromSettings(Context,int)}
     ** @see <a href="https://github.com/android/platform_frameworks_base/blob/d48e0d44f6676de6fd54fd8a017332edd6a9f096/packages/SettingsLib/src/com/android/settingslib/accessibility/AccessibilityUtils.java#L55">AccessibilityUtils</a>
     */
    public boolean isAccessibilityServiceEnabled() {
        Context context = getApplicationContext();
        Class<?> accessibilityService = MainService.class;

        ComponentName expectedComponentName = new ComponentName(context, accessibilityService);

        String enabledServicesSetting = Settings.Secure.getString(context.getContentResolver(),  Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (enabledServicesSetting == null)
            return false;

        TextUtils.SimpleStringSplitter colonSplitter = new TextUtils.SimpleStringSplitter(':');
        colonSplitter.setString(enabledServicesSetting);

        while (colonSplitter.hasNext()) {
            String componentNameString = colonSplitter.next();
            ComponentName enabledService = ComponentName.unflattenFromString(componentNameString);

            if (enabledService != null && enabledService.equals(expectedComponentName))
                return true;
        }

        return false;
    }

    public static boolean isAccessibilityServiceRunning() {
        return (MainService.serviceInstance != null);
    }

    void showEnableAccessibilityService() {
        if(isAccessibilityServiceRunning()) {
            Log.i("UHO_Captions", "Accessibility service already enabled");
            helperOnRequestPermissionsResult(REQUEST_PERMISSION_ACCESSIBILITY);
            return;
        }

        String alertMessage;
        if(!isAccessibilityServiceEnabled())
            alertMessage = "Application UHO needs to be enabled in accessibility settings in order for it to work:<br><br>1) To draw captions over other windows<br>2) To display phone call captions by recording audio<br><b>App NEVER stores or sends audio or captions over internet.</b><br><br>Tap <b>Continue</b> to open settings, find app \"UHO Captions\" and turn it ON.";
        else
            alertMessage = "Application UHO has encountered an OS error. Please turn it OFF and back ON in accessibility settings.";

        Spanned spannedAlertMessage = Html.fromHtml(alertMessage, FROM_HTML_MODE_LEGACY);

        Log.i("UHO_Captions", "Accessibility service not enabled. Opening settings");
        AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
        dlgAlert.setMessage(spannedAlertMessage);
        dlgAlert.setTitle("UHO - permission (accessibility)");
        dlgAlert.setPositiveButton("Continue", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                startActivityForResult(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS), REQUEST_PERMISSION_ACCESSIBILITY);
            }
        });
        dlgAlert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                onButtonStartEnable(true);
            }
        });
        dlgAlert.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                onButtonStartEnable(true);
            }
        });
        dlgAlert.setCancelable(true);
        dlgAlert.create().show();
    }

    void downloadActiveShowWidgets(boolean active) {
        // if we're stopped and download is active:
        if(active) {
            // hides "START" button, disables Dropdown, and shows download progress
            showDownloading(true);
            // the logic below will hide 'delete' button
            conditionallyShowDeleteAsset();
        } else {
            // shows "START" button, enables Dropdown, and hides download progress
            showDownloading(false);
            // the logic below will conditionally show 'delete' button
            conditionallyShowDeleteAsset();
        }
    }

    public void sendBroadcastMessage(int msg) {
        if(!isAccessibilityServiceRunning()) {
            Log.i("UHO_Captions", "[sendBroadcastMessage] accessibility service not enabled, returning");
            return;
        }

        //Intent serviceIntent = new Intent(getApplicationContext(), MainService.class);
        //serviceIntent.putExtra("msg", msg);
        //startForegroundService(serviceIntent);
        Intent intent = new Intent(MainService.BROADCAST_EVENT_NAME);
        intent.putExtra("msg", msg);
        //intent.setPackage(getPackageName());
        LocalBroadcastManager.getInstance(getApplicationContext()).sendBroadcast(intent);
        Log.i("UHO_Captions", "[sendBroadcastMessage] just called sendBroadcast");
    }

    public class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int msg = intent.getIntExtra("msg", -1);
            Log.i("UHO_Captions", "parsing msg");
            parseMessage(msg);
            Log.i("UHO_Captions", "parsed msg");
        }

        public void parseMessage(int msg) {
            switch(msg) {
                // additionally called upon MSG_GETSTARTSTATE
                case MSG_STARTED:
                    buttonStart.setText(stringStop);
                    buttonStart.setEnabled(true);

                    buttonDeleteAsset.setVisibility(View.GONE);
                    disableDropdown();
                    break;

                // triggered when it was started and then stopped
                case MSG_FINISHED:
                    Log.i("UHO_Captions", "MSG_FINISHED");
                    parseMessage(MSG_STOPPED);
                    conditionallyShowAppReviewDialog();
                    break;

                // triggered to signalize that it's not running
                // additionally called upon MSG_GETSTARTSTATE
                case MSG_STOPPED:
                    Log.i("UHO_Captions", "MSG_STOPPED");
                    buttonStart.setText(stringStart);
                    buttonStart.setEnabled(true);
                    //downloadActiveShowWidgets(true);

                    // TODO: remove possible redundancy (download observer suffices?)
                    isDownloadActive(new DownloadInfoHandler() {
                        @Override
                        public void callback(DownloadInfoHandler.ActiveType active) {
                            if(active.equals(ActiveType.active) || active.equals(ActiveType.enqueued))
                                downloadActiveShowWidgets(true);
                            else
                                downloadActiveShowWidgets(false);
                        }
                    });
                    break;

                case MSG_STARTERROR:
                    Log.i("UHO_Captions", "Inside broadcast onReceive, got MSG_STARTERROR");
                    Toast.makeText(getApplicationContext(), "Start error.", Toast.LENGTH_LONG).show();
                    buttonStart.setText(stringStart);
                    buttonStart.setEnabled(true);
                    break;

                // additionally called upon MSG_GETSTARTSTATE
                case MSG_WAITSTART:
                    Toast.makeText(getApplicationContext(), "Starting, please wait.", Toast.LENGTH_SHORT).show();
                    buttonStart.setText(stringStart);
                    buttonStart.setEnabled(false);
                    break;

                case MSG_CAN_INITSTART:
                    runSpeechRecognitionCheckPermissions();
                    break;

                case MSG_CAN_INITSTOP:
                    //Intent serviceIntent = new Intent(getApplicationContext(), MainService.class);
                    //serviceIntent.putExtra("msg", MSG_STARTSTOP);
                    //startForegroundService(serviceIntent);
                    sendBroadcastMessage(MSG_STARTSTOP);
                    break;

                /*case MSG_CAN_OPENSETTINGS:
                    Intent activityIntent = new Intent(MainActivity.this, SettingsActivity.class);
                    startActivity(activityIntent);
                    break;

                case MSG_CANT_OPENSETTINGS:
                    Toast.makeText(getApplicationContext(), "Za odpiranje nastavitev prvo ustavite izvajanje.", Toast.LENGTH_LONG).show();
                    break;
                 */

                case MSG_CELLULAR_DIALOG:
                    showCellularDialog();
                    break;
                default:
                    throw new IllegalStateException("Unexpected value: " + msg);
            }
        }
    };

    void showCellularDialog() {
        SharedPreferences.Editor ed = prefs.edit();
        ed.putBoolean("assetConfirmationRequired", false);
        ed.apply();

        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(getApplicationContext());
        assetPackManager.showConfirmationDialog(MainActivity.this);
    }

    void conditionallyShowAppReviewDialog() {
        int showFrequency = 5;

        SharedPreferences prefs;
        prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);

        int runCount = prefs.getInt("runCount", 0);
        runCount++;

        SharedPreferences.Editor ed = prefs.edit();
        ed.putInt("runCount", runCount);
        ed.apply();

        if(runCount == 10) {
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(MainActivity.this);
            dlgAlert.setMessage(
                    HtmlCompat.fromHtml( //Html.fromHtml(
                            "Hello! :) It looks like you used application UHO Global multiple times.<br>" +
                                    "In my available free time I develop applications and devices for people with special needs.<br>" +
                                    "It would be of great help if you could support my work with a <b>donation</b>, even the smallest amount is more than welcome!<br><br>" +
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
            Log.i("UHO_Captions", "Launching review dialog");
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

    void helperOnRequestPermissionsResult(int requestCode) {
        switch (requestCode) { // this is the first we grant
            case REQUEST_PERMISSION_ACCESSIBILITY:
                if(isAccessibilityServiceRunning())
                    showEnableMicrophone();
                else {
                    Log.i("UHO_Captions", "Got REQUEST_PERMISSION_ACCESSIBILITY, but not enabled");
                    onButtonStartEnable(true);
                }
                break;
            /*case REQUEST_PERMISSION_OVERLAY:
                if(hasOverlayPermission()) {
                    // This gets called after (first) permission for overlay drawing is accepted

                    //boolean useMic = prefs.getBoolean("useMicrophone", SettingsActivity.defaultUseMicrophone);
                    //if (useMic)
                    //    showEnableMicrophone();
                    //else
                    //    showEnableMediaProjection();
                    showEnableMicrophone();
                }
                break;*/
            case REQUEST_PERMISSION_MICROPHONE:
                showEnableMediaProjection();
                //runSpeechRecognition();
                break;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        // this gets called when returning from SettingsActivity (either by back button or "complete shutdown" button)
        // (note on order of execution: this gets called before onResume)

        Log.i("UHO_Captions", "onActivityResult: requestCode = " + requestCode + "; resultCode = " + resultCode);
        if(requestCode == REQUEST_CODE_SETTINGS && resultCode == RESULT_EXIT) {
            Log.i("UHO_Captions", "onActivityResult: finishing and removing task");
            shouldExitActivity = true;
            return;
        }

        // gets called after requesting accessibility service

        helperOnRequestPermissionsResult(requestCode);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        // this callback gets called after requesting microphone permission
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        Log.i("UHO_Captions", String.format("Inside onRequestPermissionsResult with code %d", requestCode));

        if(grantResults[0] == PackageManager.PERMISSION_GRANTED)
            helperOnRequestPermissionsResult(requestCode);
        else {
            Log.i("UHO_Captions", "[onRequestPermissionsResult] mic permission not granted");
            onButtonStartEnable(true);
        }
    }

    public static boolean isHuaweiDevice() {    // (check by gpt)
        return Build.BRAND.equalsIgnoreCase("Huawei") ||
                Build.BRAND.equalsIgnoreCase("Honor"); // Honor is a Huawei sub-brand
    }

    boolean hasMicrophonePermission() {
        if(ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            Log.i("UHO_Captions", "Microphone permission already granted");
            return true;
        }

        return false;
    }

    void showEnableMicrophone() {
        if(hasMicrophonePermission()) {
            Log.i("UHO_Captions", "Already have microphone permission");
            helperOnRequestPermissionsResult(REQUEST_PERMISSION_MICROPHONE);
            return;
        }

        AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
        dlgAlert.setMessage("Application UHO Global requires your permission to capture microphone sound.");
        dlgAlert.setTitle("UHO - permission (microphone)");
        dlgAlert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                Log.i("UHO_Captions", "Requesting record audio permission");
                String[] permissions = {
                        Manifest.permission.RECORD_AUDIO
                };
                ActivityCompat.requestPermissions(MainActivity.this, permissions, REQUEST_PERMISSION_MICROPHONE);
            }
        });
        dlgAlert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                onButtonStartEnable(true);
            }
        });
        dlgAlert.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                Log.i("UHO_Captions", "[showEnableMicrophone] dlgAlert onCancel");
                onButtonStartEnable(true);
            }
        });
        dlgAlert.setCancelable(true);
        dlgAlert.create().show();
    }

    ActivityResultLauncher<Intent> mediaProjectionLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(),
            new ActivityResultCallback<ActivityResult>() {
                @Override
                public void onActivityResult(ActivityResult result) {
                    mediaProjectionResult = result;

                    mHandler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            requestingAllPermissions = false;   // this enables "onResume()" code to update UI by communicating with service

                            //boolean appInForeground = App.isAppInForeground(); //getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.RESUMED);
                            //if(!appInForeground) {
                            //    Log.i("UHO_Captions", "[mediaProjectionLauncher] Activity not in foreground");
                            //}

                            int resultCode = result.getResultCode();
                            if(resultCode != RESULT_OK) {
                                Log.i("UHO_Captions", "mediaProjectionLauncher: resultCode != RESULT_OK");
                                if(App.isAppInForeground())
                                    onButtonStartEnable(true);
                                return;
                            }

                            // run ASR using our media projection service
                            runSpeechRecognition(result);
                            //Intent resultIntent = result.getData();
                            //int resultCode = result.getResultCode();
                            //mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, resultIntent);
                            //Log.i("UHO_Captions", "Got MediaProjection token. Running speech recognition.");
                        }
                    }, 500);
                }
            }
    );

    void showEnableMediaProjection() {
        AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
        dlgAlert.setMessage("Application UHO Global requires your permission to capture sound from other applications.");
        dlgAlert.setTitle("UHO - permission (sound)");
        dlgAlert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                mediaProjectionManager = getSystemService(MediaProjectionManager.class);
                Intent captureIntent = mediaProjectionManager.createScreenCaptureIntent();
                mediaProjectionLauncher.launch(captureIntent);
            }
        });

        dlgAlert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                onButtonStartEnable(true);
            }
        });

        dlgAlert.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                onButtonStartEnable(true);
            }
        });

        dlgAlert.setCancelable(true);
        dlgAlert.create().show();
    }

    /*void setMediaProjectionAndRunService() {
        Log.i("UHO_Captions", "Pridobivam mediaProjection");
        Intent resultIntent = mediaProjectionResult.getData();
        int resultCode = mediaProjectionResult.getResultCode();
        mediaProjection = mediaProjectionManager.getMediaProjection(resultCode, resultIntent);

        //Log.i("UHO_Captions", "Pridobil MediaProjection token. Nastavljam.");
        //MainService.serviceInstance.mediaProjection = mediaProjection;
        //MainService.serviceInstance.soundSource = SoundSource.startstream;

        Intent serviceIntent = new Intent(getApplicationContext(), MainService.class);
        serviceIntent.putExtra("code", resultCode);
        startForegroundService(serviceIntent);
        Log.i("UHO_Captions", "(Znova?) zagnal foreground service");
        buttonStart.setText("Ustavi");
    }*/

    void runSpeechRecognition(ActivityResult result) {
        /*if(MainService.serviceInstance == null) {
            Log.i(UHO1", "Service hasn't been started yet. Calling startForegroundService and waiting for broadcast");
            Intent serviceIntent = new Intent(getApplicationContext(), MainService.class);
            startForegroundService(serviceIntent);
        } else {
            setMediaProjectionAndRunService();
        }*/


        Intent resultData = result.getData();
        int resultCode = result.getResultCode();

        Intent serviceIntent = new Intent(MainService.BROADCAST_EVENT_NAME); //new Intent(getApplicationContext(), MainService.class);
        serviceIntent.putExtra("msg", MSG_STARTSTOP);
        serviceIntent.putExtra("code", resultCode);
        serviceIntent.putExtra("data", resultData);
        serviceIntent.setPackage(getPackageName());
        LocalBroadcastManager.getInstance(getApplicationContext()).sendBroadcast(serviceIntent);
        //startForegroundService(serviceIntent);
        Log.i("UHO_Captions", "Started foreground service");
    }

    void onButtonStartEnable(boolean enable) {
        if(enable) {
            buttonStart.setEnabled(true);
            conditionallyShowDeleteAsset();
            enableDropdown();

            requestingAllPermissions = false;
        } else {
            // disable some widgets
            buttonStart.setEnabled(false);
            conditionallyShowDeleteAsset();
            disableDropdown();

            requestingAllPermissions = true;
        }
    }

    void showEnableAll() {
        //showEnableOverlay();
        onButtonStartEnable(false);
        showEnableAccessibilityService();
    }

    void runSpeechRecognitionCheckPermissions() {
        showEnableAll();
    }
}
