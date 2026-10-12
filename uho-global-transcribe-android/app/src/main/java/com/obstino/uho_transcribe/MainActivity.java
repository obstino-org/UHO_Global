package com.obstino.uho_transcribe;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.util.Log;
import android.util.TypedValue;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
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

import com.google.android.gms.tasks.OnCanceledListener;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.Task;
import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;
import com.google.common.util.concurrent.ListenableFuture;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

public class MainActivity extends AppCompatActivity {
    SharedPreferences prefs;
    static String PREFS_NAME = "PREFS_UHO_Transcribe";
    static String DEFAULT_CHOSEN_ASSET = "assets_en";

    TextView textView_selected;
    ListView listView;
    Button buttonStart;
    Button buttonCancelDownload;
    TextView textViewDownloading;
    ProgressBar progressBarDownload;
    Button buttonDeleteAsset;

    String BTN_START_NOTINSTALLED = "DOWNLOAD & START";
    String BTN_START_INSTALLED = "START";

    String uniqueWorkName = "dlwork";
    String uniqueWorkTag = "dltag";

    static final int MSG_CELLULAR_DIALOG = 100;
    static final String BROADCAST_EVENT_NAME = "MainActivityBroadcast";

    MyBroadcastReceiver myBroadcastReceiver = new MyBroadcastReceiver();

    static enum TokenizerType {
        none,
        RoBERTa,
        BERT,
    };

    static class LanguageInfo {
        String listviewText;
        String assetName;
        String asrModelPath;
        String asrVocabPath;
        String pncModelPath;
        String pncVocabPath;
        boolean installTime;
        boolean isConformer;
        float timeBias;
        boolean unspacedText;
        boolean punctuate;
        TokenizerType pncTokType;
        char[] pncSymbols;
        boolean pncUseCapitalLetters;
        boolean suppressTokens;

        LanguageInfo(
                String listviewText,
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
                boolean suppressTokens
                )
        {
            this.listviewText = listviewText;
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
        }
    };

    static List<LanguageInfo> langInfo = List.of(
            new LanguageInfo("Arabic (ar-AR)", "assets_ar", "ASRMODEL_Conformer-CTC-L_spe128_ar-AR_3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe128_ar-AR_3.0.txt", "", "", false, true, 0.024f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Belarusian (be-BY)", "assets_by", "ASRMODEL_stt_by_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_by_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Brazilian Portuguese (pt-BR)", "assets_pt", "ASRMODEL_Conformer-CTC-L_bpe1024_pt-BR_1.2.int8.onnx", "ASRVOCAB_Conformer-CTC-L_bpe1024_pt-BR_1.2.txt", "PNCMODEL_bert-base_pt-BR_Riva-PnC-SET-1.0_quantized_weights.onnx", "PNCVOCAB_pt.txt", false, true, 0.024f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            new LanguageInfo("Catalan (ca)", "assets_ca", "ASRMODEL_stt_ca_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_ca_conformer_ctc_large.txt", "", "", false, true, 0.07f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Chinese (zh-CN)-English (en-US)", "assets_zh_en", "ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Croatian (hr)", "assets_hr", "ASRMODEL_stt_hr_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_hr_conformer_ctc_large.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Dutch (nl)", "assets_nl", "ASRMODEL_Conformer-CTC-L_spe256_nl-NL_1.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe256_nl-NL_1.0.txt", "PNCMODEL_bert-base-nl-NL_1.0_quantized_weights.onnx", "PNCVOCAB_nl.txt", false, true, 0.100f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            new LanguageInfo("English (en-US)", "assets_en", "ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt", "PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx", "PNCVOCAB_en.txt", true, true, 0f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            new LanguageInfo("Esperanto (eo)", "assets_eo", "ASRMODEL_stt_eo_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_eo_conformer_ctc_large.txt", "", "", false, true, 0.035f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("French (fr)", "assets_fr", "ASRMODEL_stt_fr_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_fr_conformer_ctc_large.txt", "PNCMODEL_bert-base_fr_fr_Riva-PnC-SET-2.0_quantized_weights.onnx", "PNCVOCAB_fr.txt", false, true, 0.1f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '-', '.', '?'}, true, false),
            new LanguageInfo("German (de)", "assets_de", "ASRMODEL_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.txt", "PNCMODEL_punct_de_quantized_weights.onnx", "PNCVOCAB_de.txt", false, true, 0.0f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            //new LanguageInfo("Hindi (hi)", "assets_hi", "ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt", "PNCMODEL_bert-base_hi-IN_Riva-PnC-SET-1.0_opt_quantized_weights.onnx", "PNCVOCAB_hi.txt", true, true, 0f, false, true, TokenizerType.BERT, new char[] {'\0', '!', ',', '?', '।'}, false, false),
            new LanguageInfo("Hindi (hi)", "assets_hi", "ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt", "", "", false, true, 0f, false, false, TokenizerType.none, new char [] {}, false, false),
            new LanguageInfo("Italian (it)", "assets_it", "ASRMODEL_Conformer-CTC-L_spe1024_it-IT_2.1.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe1024_it-IT_2.1.txt", "PNCMODEL_bert-base_it-IT_Riva-PnC-SET-1.1_quantized_weights.onnx", "PNCVOCAB_it.txt", false, true, 0.100f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            new LanguageInfo("Japanese (ja-JP)-English (en-JP)", "assets_ja_en", "ASRMODEL_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.txt", "", "", false, true, -0.016f, true, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Kinyarwanda (rw)", "assets_rw", "ASRMODEL_stt_rw_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_rw_conformer_ctc_large.txt", "", "", false, true, 0.14f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Korean (ko-KR)", "assets_ko", "ASRMODEL_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.txt", "PNCMODEL_bert-base_ko-KR_Riva-PnC-SET-1.1_quantized_weights.onnx", "PNCVOCAB_ko.txt", false, true, -0.024f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, false, false),
            new LanguageInfo("Marathi (mr)", "assets_mr", "ASRMODEL_stt_mr_conformer_ctc_medium.int8.onnx", "ASRVOCAB_stt_mr_conformer_ctc_medium.txt", "", "", false, true, -0.135f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Persian (fa)", "assets_fa", "ASRMODEL_stt_fa_fastconformer_hybrid_large.int8.onnx", "ASRVOCAB_stt_fa_fastconformer_hybrid_large.txt", "", "", false, false, -0.056f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Polish (pl)", "assets_pl", "ASRMODEL_stt_pl_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_pl_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0.237f, false, false, TokenizerType.none, new char[] {}, false, false),
            new LanguageInfo("Russian (ru)", "assets_ru", "ASRMODEL_stt_ru_conformer_ctc_large.int8.onnx", "ASRVOCAB_stt_ru_conformer_ctc_large.txt", "PNCMODEL_bert-base_ru-RU_Riva-PnC-SET-1.0_quantized_weights.onnx", "PNCVOCAB_ru.txt", false, true, -0.016f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?'}, true, false),
            new LanguageInfo("Slovenian (sl-SI)", "assets_sl", "ASRMODEL_sl.int8.onnx", "ASRVOCAB_sl.txt", "PNCMODEL_punct_sl_quantized_weights.onnx", "PNCVOCAB_sl.txt", false, true, 0f, false, true, TokenizerType.RoBERTa, new char[] {'\0', '!', ',', '.', '?'}, true, true),
            new LanguageInfo("Spanish (es-US)-English (en-US)", "assets_es_en", "ASRMODEL_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.int8.onnx", "ASRVOCAB_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.txt", "PNCMODEL_bert-base_es-US_Riva-PnC-SET-2.0_quantized_weights.onnx", "PNCVOCAB_es.txt", false, true, -0.016f, false, true, TokenizerType.BERT, new char[] {'\0', ',', '.', '?', '¿'}, true, false),
            new LanguageInfo("Ukrainian (ua)", "assets_ua", "ASRMODEL_stt_ua_fastconformer_hybrid_large_pc.int8.onnx", "ASRVOCAB_stt_ua_fastconformer_hybrid_large_pc.txt", "", "", false, false, 0f, false, false, TokenizerType.none, new char[] {}, false, false)
            );

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        /*ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });*/

        if (!isTaskRoot()) {    // (source: https://stackoverflow.com/a/16447508)
            // Android launched another instance of the root activity into an existing task
            //  so just quietly finish and go away, dropping the user back into the activity
            //  at the top of the stack (ie: the last state of this task)
            finish();
            return;
        }

        Toolbar toolbar = findViewById(R.id.main_toolbar);
        setSupportActionBar(toolbar);

        prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);

        textView_selected = findViewById(R.id.textview_selected2);
        listView = findViewById(R.id.listview_languages);
        buttonStart = findViewById(R.id.button_start);
        buttonCancelDownload = findViewById(R.id.button_cancel_download);
        textViewDownloading = findViewById(R.id.textview_downloading);
        progressBarDownload = findViewById(R.id.progress_download);
        buttonDeleteAsset = findViewById(R.id.button_delete_asset);

        disableListView();
        buttonStart.setVisibility(View.VISIBLE);
        buttonCancelDownload.setVisibility(View.INVISIBLE);
        textViewDownloading.setVisibility(View.INVISIBLE);
        progressBarDownload.setVisibility(View.INVISIBLE);
        buttonDeleteAsset.setVisibility(View.INVISIBLE);

        findViewById(R.id.button_donate_main).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Uri uri = Uri.parse(getString(R.string.donate));
                Intent intent = new Intent(Intent.ACTION_VIEW, uri);
                startActivity(intent);
            }
        });

        ((TextView)findViewById(R.id.textview_privacy)).setMovementMethod(LinkMovementMethod.getInstance());

        ArrayAdapter<String> arr;

        ArrayList<String> languages = new ArrayList<>();
        for(LanguageInfo info: langInfo) {
            languages.add(info.listviewText);
        }

        arr = new ArrayAdapter<String>(this,
                R.layout.listview_custom_layout, R.id.mycustomtextview, languages);
        listView.setAdapter(arr);

        /*
        // set listview border
        GradientDrawable border = new GradientDrawable();
        border.setStroke(3, MaterialColors.getColor(listView, com.google.android.material.R.attr.colorOutline));
        border.setCornerRadius(12);
        listView.setPadding(3, 3, 3, 3); // Prevents text/items from clipping the border
        */
        
        setSelected();

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
                        Log.i("UHO_Transcribe", "[listLiveData:onChanged] ENQUEUED");
                        showDownloading(true);
                        updateProgressBar(0);
                        textViewDownloading.setText("Downloading language (" + 0 + "%)");
                        break;
                    case RUNNING:
                        showDownloading(true);
                        Data progress = workInfo.getProgress();
                        int value = progress.getInt(Downloader.PROGRESS, 0);
                        Log.i("UHO_Transcribe", "download percentage = " + value);
                        updateProgressBar(value);
                        textViewDownloading.setText("Downloading language (" + value + "%)");
                        break;
                    case FAILED:
                    case BLOCKED:
                    case CANCELLED:
                        if(App.downloaderJustFinished) {
                            App.downloaderJustFinished = false;
                            Log.i("UHO_Transcribe", "download error");
                            Toast.makeText(getApplicationContext(), "Error downloading: " + workInfo.getState().toString(), Toast.LENGTH_SHORT).show();
                            showDownloading(false);
                        }
                        break;
                    case SUCCEEDED:
                        //if(prefs.getBoolean("DownloadJustFinished", false)) {
                        if(App.downloaderJustFinished) {
                            // set 'just finished' flag to false, so it doesn't auto-start next time
                            App.downloaderJustFinished = false;
                            //SharedPreferences.Editor ed = prefs.edit();
                            //ed.putBoolean("DownloadJustFinished", false);
                            //ed.apply();
                            Log.i("UHO_Transcribe", "download just finished and successful");
                            Toast.makeText(getApplicationContext(), "Download finished. Starting...", Toast.LENGTH_SHORT).show();
                            showDownloading(false);
                            startTranscribeActivity();
                        }
                        //}
                        break;
                }
            }
        });

        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                view.setSelected(true);

                LanguageInfo li = langInfo.get(position);
                SharedPreferences.Editor ed = prefs.edit();
                ed.putString("assetName", li.assetName);
                Log.i("UHO_Transcribe", "inside onItemClick. Putting " + li.assetName + " into shared prefs");
                ed.apply();

                if(isAssetPackInstalled(li.assetName)) {
                    buttonStart.setText(BTN_START_INSTALLED);
                } else {
                    buttonStart.setText(BTN_START_NOTINSTALLED);
                }

                setTextView_selected();
                conditionallyShowDeleteAsset();
            }
        });

        buttonStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                LanguageInfo li = langInfo.get(getSelectedPosition());
                if(isAssetPackInstalled(li.assetName)) {
                    Log.i("UHO_Transcribe", "Asset pack installed, starting!");
                    startTranscribeActivity();
                } else {
                    Log.i("UHO_Transcribe", "Creating download Worker");

                    //SharedPreferences.Editor ed = prefs.edit();
                    //ed.putBoolean("DownloadJustFinished", false);
                    //ed.apply();

                    WorkManager workManager = WorkManager.getInstance(getApplicationContext());
                    Data inputData = new Data.Builder()
                            .putString("packname", getListViewAssetName())
                            .build();

                    OneTimeWorkRequest workRequest = new OneTimeWorkRequest.Builder(Downloader.class)
                            .setInputData(inputData)
                            .addTag(uniqueWorkTag)
                            .setExpedited(OutOfQuotaPolicy.RUN_AS_NON_EXPEDITED_WORK_REQUEST)
                            .build();

                    workManager.enqueueUniqueWork(uniqueWorkName, ExistingWorkPolicy.REPLACE, workRequest);
                    showDownloading(true);
                }
            }
        });

        buttonCancelDownload.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
                //Context applicationContext = getApplicationContext();
                //AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(applicationContext);
                //assetPackManager.cancel(Collections.singletonList(asset));

                //cancel download of asset
                WorkManager workManager = WorkManager.getInstance(getApplicationContext());
                Operation op  = workManager.cancelUniqueWork(uniqueWorkName);
                op.getResult().addListener(new Runnable() {
                    @Override
                    public void run() {
                        showDownloading(false);
                    }
                }, ContextCompat.getMainExecutor(MainActivity.this));
            }
        });

        buttonDeleteAsset.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AlertDialog alertDialog = new AlertDialog.Builder(MainActivity.this).create();
                alertDialog.setTitle("Warning");
                alertDialog.setMessage("Are you sure you want to remove language files for \"" + langInfo.get(getSelectedPosition()).listviewText + "\"?");
                alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, "Yes",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                deleteSelectedAsset();
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

        CheckBox checkBox = findViewById(R.id.checkbox_experimental);
        checkBox.setChecked(prefs.getBoolean("enableExperimental", false));
        checkBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(@NonNull CompoundButton buttonView, boolean isChecked) {
                SharedPreferences.Editor ed = prefs.edit();
                ed.putBoolean("enableExperimental", checkBox.isChecked());
                ed.apply();
            }
        });

        conditionallyShowDeleteAsset();

        isDownloadActive(new DownloadInfoHandler() {
            @Override
            public void callback(DownloadInfoHandler.ActiveType active) {
                if(active.equals(ActiveType.active) || active.equals(ActiveType.enqueued))
                    showDownloading(true);
                else
                    showDownloading(false);
            }
        });
    }

    @Override
    protected void onRestart() {
        super.onRestart();

        conditionallyShowDeleteAsset();
    }


    @Override
    protected void onStart() {
        super.onStart();

        Log.i("UHO_Transcribe", "[onStart]");

        LocalBroadcastManager.getInstance(getApplicationContext()).registerReceiver(myBroadcastReceiver,
                new IntentFilter(MainActivity.BROADCAST_EVENT_NAME));
    }

    @Override
    protected void onStop() {
        super.onStop();

        try {
            LocalBroadcastManager.getInstance(getApplicationContext()).unregisterReceiver(myBroadcastReceiver);
        } catch (IllegalArgumentException e) {
            Log.i("UHO_Transcribe", "[MainActivity] unregisterReceiver exception");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        isDownloadActive(new DownloadInfoHandler() {
            @Override
            public void callback(ActiveType type) {
                boolean cellularDialogRequired = prefs.getBoolean("assetConfirmationRequired", false);
                if(type.equals(ActiveType.active) && cellularDialogRequired)
                    showCellularDialog();
            }
        });
    }

    public class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int msg = intent.getIntExtra("msg", -1);
            if(msg == MSG_CELLULAR_DIALOG)
                showCellularDialog();
        }
    };

    void showCellularDialog() {
        SharedPreferences.Editor ed = prefs.edit();
        ed.putBoolean("assetConfirmationRequired", false);
        ed.apply();

        Log.i("UHO_Transcribe", "[showCellularDialog] calling showConfirmationDialog");

        AssetPackManager assetPackManager = AssetPackManagerFactory.getInstance(getApplicationContext());
        assetPackManager.showConfirmationDialog(MainActivity.this);
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
            buttonDeleteAsset.setVisibility(View.INVISIBLE);
        }

        if(isAssetPackInstalled(asset)) {
            buttonStart.setText(BTN_START_INSTALLED);
        } else {
            buttonStart.setText(BTN_START_NOTINSTALLED);
        }
    }

    void deleteSetEnabled(boolean enableViews) {
        conditionallyShowDeleteAsset();

        if(enableViews) {
            enableListView();
            buttonStart.setEnabled(true);
        } else {
            Log.i("UHO_Transcribe", "deleting, setting disabled");
            disableListView();
            buttonStart.setEnabled(false);
        }
    }

    void startTranscribeActivity() {
        // check if enable experimental features
        CheckBox checkBox = findViewById(R.id.checkbox_experimental);
        SharedPreferences.Editor ed = prefs.edit();
        ed.putBoolean("enableExperimental", checkBox.isChecked());
        ed.apply();

        Intent intent = new Intent(MainActivity.this, TranscribeActivity.class);
        startActivity(intent);
    }

    int getSelectedPosition() {
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        for(int i = 0; i < langInfo.size(); i++) {
            if(langInfo.get(i).assetName.equals(asset))
                return i;
        }

        return 0;
    }

    void disableListView() {
        listView.setEnabled(false);
        listView.setBackgroundColor(Color.GRAY);
    }

    void enableListView() {
        listView.setEnabled(true);
        TypedValue typedValue = new TypedValue();
        getTheme().resolveAttribute(android.R.attr.colorBackground, typedValue, true);
        listView.setBackgroundColor(typedValue.data);
    }

    // when item selected in listview, check if language is already installed
    //      if not installed, set appropriate text in button ("Download & Start") vs ("Start")
    // (later?) consider adding delete language button?

    void showDownloading(boolean downloading) {
        setSelected();

        if(downloading) {
            Log.i("UHO_Transcribe", "downloading, setting disabled");
            disableListView();
            buttonStart.setVisibility(View.INVISIBLE);
            buttonCancelDownload.setVisibility(View.VISIBLE);
            textViewDownloading.setVisibility(View.VISIBLE);
            progressBarDownload.setVisibility(View.VISIBLE);
        } else {
            Log.i("UHO_Transcribe", "setting enabled");
            enableListView();
            buttonStart.setVisibility(View.VISIBLE);
            buttonCancelDownload.setVisibility(View.INVISIBLE);
            textViewDownloading.setVisibility(View.INVISIBLE);
            progressBarDownload.setVisibility(View.INVISIBLE);
        }
    }

    void updateProgressBar(int percent) {
        progressBarDownload.setProgress(percent);
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
                        //Log.i("UHO_Transcribe", "download percentage = " + value);
                        break;
                    case FAILED:
                    case BLOCKED:
                    case CANCELLED:
                        //Log.i("UHO_Transcribe", "download error");
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                        break;
                    case SUCCEEDED:
                        //Log.i("UHO_Transcribe", "download successful");
                        downloadInfoHandler.callback(DownloadInfoHandler.ActiveType.inactive);
                        break;
                }
            }
        }, ContextCompat.getMainExecutor(this));
    }

    void setInitialWidgetStates() {
        disableListView();
        buttonStart.setVisibility(View.VISIBLE);
        buttonStart.setEnabled(false);  // todo setEnabled to true when appropriate
        buttonCancelDownload.setVisibility(View.INVISIBLE);
        textViewDownloading.setVisibility(View.INVISIBLE);
        progressBarDownload.setVisibility(View.INVISIBLE);
    }

    String getListViewAssetName() {
        int idxSelected = 0;
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        for(int i = 0; i < langInfo.size(); i++) {
            LanguageInfo li = langInfo.get(i);
            if(li.assetName.equals(asset))
                idxSelected = i;
        }

        return langInfo.get(idxSelected).assetName;
    }

    void setTextView_selected() {
        int idxSelected = 0;
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        for(int i = 0; i < langInfo.size(); i++) {
            LanguageInfo li = langInfo.get(i);
            if(li.assetName.equals(asset))
                idxSelected = i;
        }
        textView_selected.setText(langInfo.get(idxSelected).listviewText);
    }
    // call this function after listview has been populated
    void setSelected() {
        int idxSelected = 0;
        String asset = prefs.getString("assetName", DEFAULT_CHOSEN_ASSET);
        for(int i = 0; i < langInfo.size(); i++) {
            LanguageInfo li = langInfo.get(i);
            if(li.assetName.equals(asset))
                idxSelected = i;
        }

        final int finalIndex = idxSelected;
        listView.postDelayed(new Runnable() {
            @Override
            public void run() {
                //listView.setSelection(finalIndex);
                int offset = 0;
                View v = listView.getChildAt(0);
                if(v != null)
                    offset = v.getHeight() / 2;
                listView.setItemChecked(finalIndex, true);
                listView.smoothScrollToPositionFromTop(finalIndex, offset);
            }
        }, 300);

        setTextView_selected();

        if(isAssetPackInstalled(asset)) {
            buttonStart.setText(BTN_START_INSTALLED);
        } else {
            buttonStart.setText(BTN_START_NOTINSTALLED);
        }
    }
}
