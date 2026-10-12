// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

package com.obstino.uho_captions;

import static com.obstino.uho_captions.MainActivity.isAccessibilityServiceRunning;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.RadioButton;
import android.widget.Toast;

//import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
//import androidx.localbroadcastmanager.content.LocalBroadcastManager;

public class SettingsActivity extends AppCompatActivity {
    //static int [] fontSizes = { 10, 15, 20, 30 };
    static int [] fontSizes = { 14, 18, 24, 32 };
    static int defaultFontSetting = 1; // Accepted values 0 (small), 1 (normal), 2 (medium), 3 (large)
    //static boolean defaultUseMicrophone = false;
    static int numLinesSmall = 2;
    static int numLinesLarge = 5;

    SharedPreferences prefs;

    RadioButton radioSmallFont;
    RadioButton radioNormalFont;
    RadioButton radioMediumFont;
    RadioButton radioLargeFont;

    Button buttonExit;
    CheckBox checkBox;

    boolean enableCheckBox = false;

    MyBroadcastReceiver myBroadcastReceiver = new MyBroadcastReceiver();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //EdgeToEdge.enable(this);
        setContentView(R.layout.activity_settings);

        prefs = getSharedPreferences(MainActivity.PREFS_NAME, MODE_PRIVATE);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar_settings);
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //onBackPressed();
                getOnBackPressedDispatcher().onBackPressed();
            }
        });

        radioSmallFont = findViewById(R.id.radiobutton_small);
        radioNormalFont = findViewById(R.id.radiobutton_normal);
        radioMediumFont = findViewById(R.id.radiobutton_medium);
        radioLargeFont = findViewById(R.id.radiobutton_large);

        // Set font size GUI settings
        int fontSetting = prefs.getInt("fontSetting", defaultFontSetting);
        if(fontSetting < 0 || fontSetting >= fontSizes.length)  // clip just in case
            fontSetting = 1;

        switch(fontSetting) {
            case 0:
                radioSmallFont.setChecked(true);
                break;
            case 1:
                radioNormalFont.setChecked(true);
                break;
            case 2:
                radioMediumFont.setChecked(true);
                break;
            case 3:
                radioLargeFont.setChecked(true);
                break;
        }

        radioSmallFont.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(!isChecked)
                    return;
                SharedPreferences.Editor editor = prefs.edit();
                editor.putInt("fontSetting", 0);
                editor.apply();
            }
        });

        radioNormalFont.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(!isChecked)
                    return;
                SharedPreferences.Editor editor = prefs.edit();
                editor.putInt("fontSetting", 1);
                editor.apply();
            }
        });

        radioMediumFont.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(!isChecked)
                    return;
                SharedPreferences.Editor editor = prefs.edit();
                editor.putInt("fontSetting", 2);
                editor.apply();
            }
        });

        radioLargeFont.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(!isChecked)
                    return;
                SharedPreferences.Editor editor = prefs.edit();
                editor.putInt("fontSetting", 3);
                editor.apply();
            }
        });

        buttonExit = findViewById(R.id.button_exit);
        buttonExit.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                new AlertDialog.Builder(SettingsActivity.this)
                        .setTitle("App full shutdown")
                        .setMessage(
                                "Are you sure that you want to completely shut down the app?\n" +
                                "In such case it will need to be reenabled in accessibility settings the next time you run it.")
                        .setPositiveButton("Yes", (dialog, which) -> {
                            Log.i("UHO_Settings", "Full shutdown triggered");
                            sendBroadcastMessage(MainActivity.MSG_FORCESTOP);
                            setResult(MainActivity.RESULT_EXIT);
                            finish();
                        })
                        .setNegativeButton("No", (dialog, which) -> {
                            // Stay in parent activity
                            dialog.dismiss();
                        })
                        .show();
            }
        });

        checkBox = findViewById(R.id.checkbox_experimental);
        //checkBox.setEnabled(false);
        checkBox.setChecked(prefs.getBoolean("enableExperimental", false));
        checkBox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!enableCheckBox) {
                    Toast.makeText(getApplicationContext(), "Can't change while running.\nPress 'STOP' in main menu first.", Toast.LENGTH_LONG).show();
                    checkBox.setChecked(!checkBox.isChecked());
                    return;
                }

                SharedPreferences.Editor ed = prefs.edit();
                ed.putBoolean("enableExperimental", checkBox.isChecked());
                ed.apply();
            }
        });
        /*checkBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(@NonNull CompoundButton buttonView, boolean isChecked) {
                SharedPreferences.Editor ed = prefs.edit();
                ed.putBoolean("enableExperimental", checkBox.isChecked());
                ed.apply();
            }
        });*/

        //if(!isAccessibilityServiceRunning())
        //    checkBox.setEnabled(true);
        if(!isAccessibilityServiceRunning())
            enableCheckBox = true;

        else
            sendBroadcastMessage(MainActivity.MSG_GETSTARTSTATE);
    }

    @Override
    protected void onStart() {
        super.onStart();
        LocalBroadcastManager.getInstance(getApplicationContext()).registerReceiver(myBroadcastReceiver,
                new IntentFilter(MainActivity.BROADCAST_EVENT_NAME));
    }

    @Override
    protected void onStop() {
        super.onStop();
        try {
            LocalBroadcastManager.getInstance(getApplicationContext()).unregisterReceiver(myBroadcastReceiver);
        } catch (IllegalArgumentException e) {
            Log.i("UHO_Captions", "[SettingsActivity] unregisterReceiver exception");
        }
    }

    void sendBroadcastMessage(int msg) {
        if(!isAccessibilityServiceRunning())
            return;

        //Intent serviceIntent = new Intent(getApplicationContext(), MainService.class);
        //serviceIntent.putExtra("msg", msg);
        //startForegroundService(serviceIntent);
        Intent intent = new Intent(MainService.BROADCAST_EVENT_NAME);
        intent.putExtra("msg", msg);
        //intent.setPackage(getPackageName());
        LocalBroadcastManager.getInstance(getApplicationContext()).sendBroadcast(intent);
    }

    public class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int msg = intent.getIntExtra("msg", -1);
            if(msg == MainActivity.MSG_STOPPED)
                enableCheckBox = true; //checkBox.setEnabled(true);
        }
    }
}