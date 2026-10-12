# Application *UHO Global*
## Live Captions for Deaf & Hard of Hearing



This **free** and **open-source** application is available for Android and Windows systems.

It was developed under *Obstino - Assistive Technology Development Association*.

**Learn more and get the app**: <https://www.obstino.org/uho-global>

---

There are three available UHO Global app variants:

- UHO Global - captions for deaf (Android)
  - captures **sound from other apps** to generate *live captions* using speech recognition
- UHO Global - deaf transcribe (Android)
  - captures speech **from microphone** and show *live transcript* using speech recognition
- UHO Global for Windows
  - *displays transcript and captions* with speech recognition, using **microphone** or **speakers** as the sound source

<!--*Click to watch* the Android app in action:  
<a href="https://www.youtube.com/watch?v=9FZV-gaD5mE&list=PLVfkPEpEpDS4&index=1">
  <img src="https://img.youtube.com/vi/9FZV-gaD5mE/0.jpg" width="80%" alt="Watch UHO Android in action">
</a>-->

*Click to watch* the demonstration of Android app **UHO Global - captions for deaf**<br>
<a href="https://www.youtube.com/watch?v=o4TwT8qly3k">
  <img src="https://img.youtube.com/vi/o4TwT8qly3k/0.jpg" width="75%" alt="Watch demo of UHO Global - captions for deaf">
</a>

*Click to watch* the demonstration of Android app **UHO Global - deaf transcribe**<br>
<a href="https://www.youtube.com/watch?v=9FZV-gaD5mE">
  <img src="https://img.youtube.com/vi/9FZV-gaD5mE/0.jpg" width="75%" alt="Watch demo of UHO Global - deaf transcribe">
</a>

*Click to watch* the demonstration of app **UHO Global for Windows**<br>
<a href="https://www.youtube.com/watch?v=RySQx19ZCHk">
  <img src="https://img.youtube.com/vi/RySQx19ZCHk/0.jpg" width="75%" alt="Watch demo of UHO Global for Windows">
</a>

### How this repository is organized

Code can be found here:

- Directory `/uho-global-transcribe-android` contains Android source code for <i>UHO&nbsp;Global&nbsp;&#8209;&nbsp;deaf&nbsp;transcribe</i>
- Directory `/uho-global-captions-android` contains Android source code for <i>UHO&nbsp;Global&nbsp;&#8209;&nbsp;captions&nbsp;for&nbsp;deaf</i>
- Directory `/uho-global-windows` contains source code for Windows variant of <i>UHO&nbsp;Global</i>

Relevant scripts:

- Directory `/py_prepare_asr`  contains notebook that was used to prepare NVIDIA NeMo Automatic Speech Recognition (ASR) models and vocabularities for ONNX inference. It also contains a simple ASR ONNX inference script and a script to prepare log mel filters (melfilters.bin) for ASR inference in C++
  - Normally you don't need to prepare or use this script, because all models have already been made available under Releases on this repository
- Directory `/py_prepare_punct` contains jupyter notebooks that were used to prepare NVIDIA NeMo Punctuation and Capitalization (PnC) models and vocabularities for ONNX inference. It also contains a simple PnC ONNX inference script
  - Normally you don't need to prepare or use this script, because all models have already been made available under Releases on this repository
- Directory `/py_prepare_vad` contains a jupyter notebook that fetches NVIDIA NeMo MarbletNet2 Voice Activity Detection (VAD) model, converts it to ONNX, and performs ONNX inference
- Directory `/py_prepare_sortformer` contains a jupyter notebook to quantize NVIDIA NeMo Sortformer speaker diarization model and perform sample inference. There is also a script to prepare mel filters file (melfilters128.bin) used in C++ inference code

### Assets in this repository

  - [click here](https://github.com/obstino-org/UHO_Global/releases#release-v3.0.0-assets1) to download AI assets required to compile Android app
  - [click here](https://github.com/obstino-org/UHO_Global/releases#release-v3.0.0-assets2) to obtain Windows app AI assets
    - (normally don't need to download these, as the Windows app will download them upon user request)
  - [click here](https://github.com/obstino-org/UHO_Global/releases#release-v3.0.0-windows-installer) to download Windows installer or zip with all relevant dependencies

### Important notes on UHO for Android

The Android Studio used was *"Android Studio Quail 3 | 2026.1.3 Patch 1"*

Compiling and running the app requires a special approach, because the app uses "assets packs" (Play Asset Delivery - PAD) to store speech recognition and related AI models/assets. Asset packs are in use because otherwise an app's size would be too big to put on Google Play store.

In both Android app variants we use on-demand asset packs for all languages except English, which is an install time asset pack.
On-demand asset packs get uploaded to Play Store and are downloaded from there when user selects a language, whereas English in this case gets downloaded as part of the app, which means users can readily use the English speech recognition as soon as the app downloads. This setting (on-demand vs install-time) can be changed by modifying build.gradle file within folder that holds those assets. For example, for English, `PROJECT_BASE_FOLDER/assets_en/build.gradle` file would be modified.

#### How to compile and run the app

Get the assets (look under Releases [here](https://github.com/obstino-org/UHO_Global/releases#release-v3.0.0-assets1)) and copy them to Android project base folder. Then refer to `COMPILE_ADB_UHO.txt` file on how to compile and run the app using bundletool.

**NOTE 1:** include only assets which you're testing for. You do this by modifying `PROJECT_BASE_FOLDER/settings.gradle` and `PROJECT_BASE_FOLDER/app/build.gradle`<br>
**NOTE 2:** bundletool approach of compiling and running the app is normally required because pressing "Run" button inside Android Studio will not work. When you press Run, only assets inside `PROJECT_BASE_FOLDER/app/src/main/assets` will be recognized by the app. Normally one wouldn't copy required asset files to that folder, but for testing purposes it is a valid choice. If you choose that testing approach for purposes of faster compilation, you must copy contents of `my_ai_pack\src\main\assets\*` and `assets_en\src\main\assets\*` (for example if English language is being tested) into that folder. When reverting from that testing back to normal bundletool approach, make sure that folder `PROJECT_BASE_FOLDER/app/src/main/assets` does not exist or rename it to something else.

### Notes on UHO Global for Windows

Development environment used was *"Microsoft Visual Studio Community 2022 (64-bit) - Version 17.14.40"* whereas libraries were previously compiled using VS 2019.

In order to compile, [download this zip file](https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-windows-installer/UHO_Windows.zip) that contains all the runtime dependencies.

Compile-time dependencies are already contained within the source code directories when you clone the repository, namely `/uho-global-windows/libs` which contain relevant .lib files.

ONNX runtime used is 1.19.2

PortAudio version used implements WASAPI loopback, and can be obtained on https://github.com/PortAudio/portaudio/tree/5d01510dc907c1791a4945c9bc4dd6d403c9a410.<br>
It was compiled using: `make .. -G "Visual Studio 16 2019" -A x64 -DPA_USE_WASAPI=ON -DPA_USE_WDMKS=ON -DPA_USE_WMME=ON -DPA_USE_DS=ON && cmake --build . --config Release`

For file download, curl 8.18.0 is used and compiled using `cmake .. -G "Visual Studio 16 2019" -A x64 -DCURL_USE_LIBPSL=OFF -DCURL_ENABLE_SSL=ON && cmake --build . --config Release`

### Other relevant information

There exists also Slovenian version of the app, called "UHO". Its github project can be found on [https://github.com/obstino-org/UHO_Slovene_v3/](https://github.com/obstino-org/UHO_Slovene_v3/)