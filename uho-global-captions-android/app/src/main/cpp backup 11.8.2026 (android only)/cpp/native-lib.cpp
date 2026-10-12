// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#include <jni.h>
#include <string>

#include <map>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <locale>
#include <codecvt>
#include <regex>
#include <deque>
#include <mutex>
#include <math.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

//#include <corecrt_math_defines.h>
#include "onnxruntime/onnxruntime_cxx_api.h"
#include "onnxruntime/nnapi_provider_factory.h"
#include "gzip/compress.hpp"
#include "nemo_inference.h"
#include "nemo_realfeed.h"
#include "nemo_punct_capit.h"
#include "nemo_sortformer.h"
#include "stream.h"

using namespace std;

shared_ptr<NeMoRealFeed> nrf = nullptr;
shared_ptr<NeMoPC> npc = nullptr;
shared_ptr<MainStream> stream = nullptr;


class LangInfo {
public:
    AAssetManager *manager;
    string asrModelPath;
    string asrVocabPath;
    string pncModelPath;
    string pncVocabPath;
    bool installTime;
    bool isConformer;
    float timeBias;
    bool unspacedText;
    bool punctuate;
    TokenizerType tokType;
    vector<wstring> pncSymbols;
    bool pncUseCapitalLetters;
    bool suppressTokens;
    bool experimental;

    LangInfo(AAssetManager *manager, string asrModelPath, string asrVocabPath, string pncModelPath, string pncVocabPath, bool installTime, bool isConformer, float timeBias, bool unspacedText, bool punctuate, TokenizerType tokType, vector<wstring> pncSymbols, bool pncUseCapitalLetters, bool suppressTokens, bool experimental):
            manager(manager), asrModelPath(asrModelPath), asrVocabPath(asrVocabPath), pncModelPath(pncModelPath), pncVocabPath(pncVocabPath), installTime(installTime), isConformer(isConformer), timeBias(timeBias), unspacedText(unspacedText), punctuate(punctuate), tokType(tokType), pncSymbols(pncSymbols), pncUseCapitalLetters(pncUseCapitalLetters), suppressTokens(suppressTokens), experimental(experimental)
    {
    }
};


enum LooperInfoASR {
    native_start_asr,
    native_stop_asr,
//    native_pause_asr,
//    native_resume_asr
};

struct ASR_State {
    LooperInfoASR state;
    void *data;
};

std::mutex asrStateLock;
std::queue<ASR_State> asrStateQueue;

wstring replaceSubstring(wstring strIn, wstring subStr, wstring replacement) {
    int idx;
    while ((idx = strIn.find(subStr)) != wstring::npos)
        strIn = strIn.replace(idx, subStr.length(), replacement);
    return strIn;
}

void newTextCallback(wstring textUTF8) {
    wstring prevFullText = stream->getText();
    wstring newFullText = textUTF8;

    auto t0 = chrono::high_resolution_clock::now();

    if (npc != nullptr) {
        wstring spkChgSymbol = L">>";	// todo receive symbol
        wstring padSymbol = L"[PAD]"; // L" <unk> ";
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "prevFullText = \"%S\", newFullText = \"%S\"\n", prevFullText.c_str(), newFullText.c_str());
        newFullText = npc->punctuateAndCapitalize(prevFullText, newFullText);
        // subsitute "[PAD]" for " >>"
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "substituting inside \"%S\" (len=%i)\n", newFullText.c_str(), newFullText.length());
        newFullText = replaceSubstring(newFullText, padSymbol, L" " + spkChgSymbol);
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "now newFullText = %S\n", newFullText.c_str());
    }
    auto t1 = chrono::high_resolution_clock::now();
    __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "P&C inference time = %lli ms", chrono::duration_cast<chrono::milliseconds>(t1-t0).count());

    stream->setText(newFullText);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_obstino_uho_1captions_MainService_nativeGetNewText(
        JNIEnv* env,
        jobject mainService) {
    wstring outText = stream->getText();
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter("");
    return env->NewStringUTF(converter.to_bytes(outText.c_str()).c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_obstino_uho_1captions_MainService_nativeResetText(
        JNIEnv* env,
        jobject mainService) {
    if(nrf == nullptr)
        return;

    nrf->resetAudioText = true;
}

extern "C" JNIEXPORT jfloat /*JNICALL*/ Java_com_obstino_uho_1captions_MainService_nativeGetTimeASR(
        JNIEnv* env,
        jobject mainService) {
    return stream->getTimeASR();
}

// interface between Java code that records audio and C++ code that will use that audio
extern "C" JNIEXPORT void JNICALL
Java_com_obstino_uho_1captions_MainService_nativeAddNewAudio(
        JNIEnv* env,
        jobject mainService,
        jfloatArray audioArray) {
    jsize audioSize = env->GetArrayLength(audioArray);
    jfloat *audio = env->GetFloatArrayElements(audioArray, nullptr);

    vector<float> audioVect = vector<float>(audio, audio + audioSize);
    stream->addNewAudio(audioVect);

    env->ReleaseFloatArrayElements(audioArray, audio, 0);
}


void thrStateLooperASR() {
    while(true) {
        asrStateLock.lock();
        if(asrStateQueue.empty()) {
            asrStateLock.unlock();
            this_thread::sleep_for(chrono::milliseconds (100));
            continue;
        }

        ASR_State asrState = asrStateQueue.front();
        asrStateQueue.pop();
        asrStateLock.unlock();

        switch(asrState.state) {
            case native_start_asr: {
                LangInfo *info = (LangInfo*)asrState.data;

                if(nrf != nullptr) {
                    delete info;
                    break;
                }

                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "Creating new MainStream object");
                stream = make_shared<MainStream>();
                stream->setText(L"Captions will be shown here.");

                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "Creating new NeMoRealFeed object");
                nrf = make_shared<NeMoRealFeed>(info->manager);
                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "Creating new NeMoPC object");

                if(info->experimental && info->punctuate && !info->pncModelPath.empty() && !info->pncVocabPath.empty())
                {
                    npc = make_shared<NeMoPC>(info->manager, info->pncModelPath, info->pncVocabPath, (TokenizerType)info->tokType, info->pncSymbols, info->pncUseCapitalLetters, info->installTime);
                    if(npc == nullptr)
                        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NPC = NULLPTR !");
                }

                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "Calling NeMoBeginFeed");
                nrf->NeMoBeginFeed(
                        info->asrModelPath,
                        info->asrVocabPath,
                        newTextCallback,
                        stream,
                        info->timeBias,
                        info->unspacedText,
                        info->punctuate,
                        info->isConformer,
                        info->installTime,
                        info->suppressTokens,
                        info->experimental
                );

                delete info;

                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "[native_start_asr] Called OK.");
            } break;
            case native_stop_asr:
                if(nrf == nullptr)
                    return;

                nrf->NeMoStopFeed();

                while(nrf->looperThread.joinable()) {
                    this_thread::sleep_for(chrono::milliseconds (100));
                }
                nrf = nullptr;
                npc = nullptr;

                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "[native_stop_asr] Stopped ok");
                break;
            /*case native_pause_asr:
                if (nrf == nullptr)
                    break;
                nrf->NeMoPauseResume(true);
                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "[native_pause_asr] Calling NeMoPauseResume");
                break;
            case native_resume_asr:
                if (nrf == nullptr)
                    break;
                nrf->NeMoPauseResume(false);
                __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "[native_resume_asr] Calling NeMoPauseResume");
                break;*/
        }
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "[JNI_OnLoad]");
    thread t(thrStateLooperASR);
    t.detach();
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_obstino_uho_1captions_MainService_nativeStartASR(
        JNIEnv* env,
        jobject mainService,
        jobject assetManager,
        jstring jStrAsrModelPath,
        jstring jStrAsrVocabPath,
        jstring jStrPncModelPath,
        jstring jStrPncVocabPath,
        jboolean installTime,
        jboolean isConformer,
        jfloat timeBias,
        jboolean unspacedText,
        jboolean punctuate,
        jint pncType,
        jcharArray pncSymbols,
        jboolean pncUseCapitalLetters,
        jboolean suppressTokens,
        jboolean experimental)
{
    AAssetManager *manager = AAssetManager_fromJava(env, assetManager);

    if(jStrAsrModelPath == nullptr || jStrAsrVocabPath == nullptr || jStrPncModelPath == nullptr || jStrPncVocabPath == nullptr || pncSymbols == nullptr)
        return;

    const char *pStrAsrModelPath = env->GetStringUTFChars(jStrAsrModelPath, nullptr);
    const char *pStrAsrVocabPath = env->GetStringUTFChars(jStrAsrVocabPath, nullptr);
    const char *pStrPncModelPath = env->GetStringUTFChars(jStrPncModelPath, nullptr);
    const char *pStrPncVocabPath = env->GetStringUTFChars(jStrPncVocabPath, nullptr);
    jsize len = env->GetArrayLength(pncSymbols);
    jchar *pCharPncSymbols = env->GetCharArrayElements(pncSymbols, nullptr);

    string asrModelPath(pStrAsrModelPath);
    string asrVocabPath(pStrAsrVocabPath);
    string pncModelPath(pStrPncModelPath);
    string pncVocabPath(pStrPncVocabPath);
    vector<wstring> pncSymbolsVect(len);
    for(int i = 0; i < len; i++) {
        wchar_t symbol = pCharPncSymbols[i];
        if(symbol == L'\0')
            pncSymbolsVect[i] = L"";
        else
            pncSymbolsVect[i] = wstring(1, (wchar_t)pCharPncSymbols[i]);
    }

    env->ReleaseStringUTFChars(jStrAsrModelPath, pStrAsrModelPath);
    env->ReleaseStringUTFChars(jStrAsrVocabPath, pStrAsrVocabPath);
    env->ReleaseStringUTFChars(jStrPncModelPath, pStrPncModelPath);
    env->ReleaseStringUTFChars(jStrPncVocabPath, pStrPncVocabPath);
    env->ReleaseCharArrayElements(pncSymbols, pCharPncSymbols, JNI_ABORT);

    LangInfo *langInfo = new LangInfo(
            manager,
            asrModelPath,
            asrVocabPath,
            pncModelPath,
            pncVocabPath,
            (installTime == JNI_TRUE),
            (isConformer == JNI_TRUE),
            timeBias,
            (unspacedText == JNI_TRUE),
            (punctuate == JNI_TRUE),
            (TokenizerType)pncType,
            pncSymbolsVect,
            (pncUseCapitalLetters == JNI_TRUE),
            (suppressTokens == JNI_TRUE),
            (experimental == JNI_TRUE)
    );

    ASR_State asrState{};
    asrState.state = native_start_asr;
    asrState.data = langInfo;

    asrStateLock.lock();
    asrStateQueue.push(asrState);
    asrStateLock.unlock();
}

extern "C" JNIEXPORT void JNICALL
Java_com_obstino_uho_1captions_MainService_nativeStopASR(
        JNIEnv* env,
        jobject mainService) {
    ASR_State asrState{};
    asrState.state = native_stop_asr;
    asrState.data = nullptr;

    asrStateLock.lock();
    asrStateQueue.push(asrState);
    asrStateLock.unlock();
}

vector<float> readWaveAsset(AAssetManager *manager, char *assetFileName) {
    AAsset *asset = AAssetManager_open(manager, assetFileName, AASSET_MODE_BUFFER);
    uint8_t *buffer = (uint8_t*)AAsset_getBuffer(asset);
    long len = AAsset_getLength(asset);

    // skip 44 bytes (headers and everything), assume we have 16kHz mono WAV file
    short *v = (short*)((unsigned long)buffer + 44);
    unsigned long vSize = (len - 44)/2;

    // convert to floating point
    vector<float> ret;
    for(int i = 0; i < vSize; i++)
        ret.push_back((float)v[i] / 32768.0f);

    return ret;
}
