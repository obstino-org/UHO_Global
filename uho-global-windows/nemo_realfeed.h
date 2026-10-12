// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#ifndef NEMO_REALFEED_H
#define NEMO_REALFEED_H

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <random>
#include <set>
//#include <io.h>	// setmode UTF8
#include <fcntl.h>	// -||-

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "logmel_nemo.h"
#include "nemo_inference.h"
#include "vad.h"
#include "nemo_vad.h"
#include "nemo_sortformer.h"

#if defined(__ANDROID__)
#include "stream.h"
#elif defined(_WIN32)
#include "stream_portaudio.h"
#endif

#include "levenshtein.h"
#include "spkchg.h"
#include "dtw.h"

#if defined(_WIN32)
bool WriteBufferIntoWav(string filePath, vector<float> in)	// assumes we have 16000 fs, 1 chan, 16 bit
{
    FILE* f;
    DWORD ChunkSize;
    DWORD Subchunk1Size;
    WORD AudioFormat;
    WORD NumChannels;
    DWORD SampleRate;
    DWORD ByteRate;
    WORD BlockAlign;
    WORD BitsPerSample;
    DWORD Subchunk2Size;

    int num = in.size();
    vector<short> sin;
    for (int i = 0; i < num; i++) {
        sin.push_back((short)(in[i] * 32767.0f));
    }

    fopen_s(&f, filePath.c_str(), "wb");
    if (!f)
        return false;

    ChunkSize = 36 + num * 2;
    fwrite("RIFF", 1, 4, f);
    fwrite(&ChunkSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    Subchunk1Size = 16;
    fwrite(&Subchunk1Size, 4, 1, f);
    AudioFormat = 1;
    fwrite(&AudioFormat, 2, 1, f);
    NumChannels = 1;
    fwrite(&NumChannels, 2, 1, f);
    SampleRate = 16000;
    fwrite(&SampleRate, 4, 1, f);
    ByteRate = SampleRate * 2;
    fwrite(&ByteRate, 4, 1, f);
    BlockAlign = 2;
    fwrite(&BlockAlign, 2, 1, f);
    BitsPerSample = 16;
    fwrite(&BitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    Subchunk2Size = num * 2;
    fwrite(&Subchunk2Size, 4, 1, f);
    fwrite((void*)sin.data(), sizeof(short), num, f);
    fclose(f);

    return true;
}
#endif

#if defined(__ANDROID__)
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
#endif

class NeMoRealFeed {
public:
#if defined(__ANDROID__)
    float captureStepSeconds = 0.6f; //0.75f;    // every captureStepSeconds, obtain new audio
    float captureWindowSeconds = 5.0f;//5.0f; //6.0f; // speech recognition audio buffer size
    float enduringSilenceCount = 2.0f;  // in TranscribeActivity, this many seconds need to be silent to print empty line
    float overlapTextWindow = 0.5f; // 2.0f // longest segment length that will be aligned (larger value corrects more text from past)

    float audioPaddingTime = 0.5f;   // how much audio will be padded on left and right

    bool removeUnfinishedWord = false;   // don't guess last word
    //float compressFactor = 1.0f;    // Experimental! better to set to 1.0
#elif defined(_WIN32)
    float captureStepSeconds = 0.6f; //0.6f; //1.0;    // 0.6 every captureStepSeconds, obtain new audio (warning too low value isn't always optimal)
    float captureWindowSeconds = 10.0f; // speech recognition audio buffer size
    float enduringSilenceCount = 2.0f;  // this many seconds need to be silent to print empty new line
    float overlapTextWindow = 0.5f; //0.5f; //0.5f;//1.0f; //1.5f; (warning, too much overlapTextWindow 'memory' gives bad results) -- originally 0.5f --- but also too little overlaptTextWindow memory gives really bad results

    float audioPaddingTime = 0.5f;

    bool removeUnfinishedWord = false;   // don't guess last word
#endif

    atomic<bool> bInferenceReady = { false };

    bool unspacedText;  // set in constructor (true for Chinese and Japanese)
    float timeBias; // set in constructor
    bool isConformer; // set in constructor
    bool suppressTokens; // set in constructor
    bool experimental; // set in constructor

    atomic<bool> stopLooperThread{ false };
    atomic<bool> pauseLooperThread{ false };
#if defined(__ANDROID__)
    atomic<bool> resetAudioText { false };
#endif
    thread looperThread;

#if defined(_WIN32)
    atomic<float> timeASR = 0.1f;   // value modified with setTimeASR and obtained with getTimeASR
#endif

#if defined(__ANDROID__)
    AAssetManager *assetManager;
#endif

    //SileroVAD vad;
    NeMoVAD nemoVad;

#if defined(__ANDROID__)
    NeMoRealFeed(AAssetManager *manager): assetManager(manager), /*vad(manager, "silero_vad_16k_op15_NEW.onnx")*/nemoVad(manager, "marblenet2.onnx", 5.0f) { }
#elif defined(_WIN32)
    NeMoRealFeed(): nemoVad(L"marblenet2.onnx", 5.0f) { }
#endif

#if defined(__ANDROID__)
    bool NeMoBeginFeed(string modelPath, string vocabPath, void (*f)(wstring fullText), shared_ptr<MainStream> stream, float timeBias=0.0f, bool unspacedText=false, bool punctuate=false, bool isConformer=false, bool installTime=true, bool suppressTokens=false, bool experimental=false) {
#elif defined(_WIN32)
        bool NeMoBeginFeed(wstring modelPath, wstring vocabPath, void (*f)(wstring fullText), bool micOrStream, float timeBias=0.0f, bool unspacedText=false, bool punctuate=false, bool isConformer=false, bool suppressTokens=false, bool experimental=false) {
#endif
        if (looperThread.joinable())
            return false;	// signalize that thread is still running, and that NeMoStopFeed should be called first

#if defined(__ANDROID__)
        setInferenceReady(false);
#endif

        this->unspacedText = unspacedText;
        this->timeBias = timeBias;
        //this->punctuate = punctuate;
        this->isConformer = isConformer;
        this->suppressTokens = suppressTokens;
        this->experimental = experimental;

        stopLooperThread = false;
#if defined(__ANDROID__)
        looperThread = thread(&NeMoRealFeed::inferenceLoopASR, this, modelPath, vocabPath, f, stream, installTime);
#elif defined(_WIN32)
        looperThread = thread(&NeMoRealFeed::inferenceLoopASR, this, modelPath, vocabPath, f, micOrStream);
#endif
        return true;
    }

    void NeMoStopFeed() {
        stopLooperThread = true;
    }

    void NeMoPauseResume(bool pause) {
        pauseLooperThread = pause;
    }

    bool NeMoIsRunning() {
        return looperThread.joinable();
    }

#if defined(_WIN32)
    // for Windows we set a public variable,
	// whereas for Android we set it using stream->setTimeASR

    void setTimeASR(float elapsedSeconds) {
        timeASR = elapsedSeconds;
    }

    float getTimeASR() {
        return timeASR;
    }
#endif

    NeMoInference::WordLevelOutput align(NeMoInference::WordLevelOutput fullText, NeMoInference::WordLevelOutput lastText)
    {
        if (lastText.timestamps.size() && fullText.timestamps.size() &&
            lastText.timestamps.back() < fullText.timestamps.back()) {
            // COMMENTED ON 3.4.2026
            //wcout << L"NOT ALIGNING: lastText is shorter than fullText!" << endl;
            //wcout << L"Last text timestamps: " << lastText.toStringTimestamps() << endl;
            //wcout << L"Full text timestamps: " << fullText.toStringTimestamps() << endl;
            //return fullText;

            // remove these 3 lines later
            //fullText.timestamps.resize(lastText.timestamps.size());
            //fullText.probs.resize(lastText.probs.size());
            //fullText.words.resize(lastText.words.size());
#if defined(_WIN32)
            wcout << L"INFO: lastText was shorter than fullText!" << endl;
#endif
        }

        NeMoInference::WordLevelOutput fullTextOut = fullText;
        NeMoInference::WordLevelOutput modLastText = lastText;

        // erase lastText's timestamps that extend beyond fullText's last timestamp
        float maxDiff = 0.3f; //0.3f;
        while (modLastText.timestamps.size() && fullText.timestamps.size()) {
            float tmpLastEndTimestamp = modLastText.timestamps[modLastText.timestamps.size() - 1];
            float fullTextEndTimestamp = fullText.timestamps[fullText.timestamps.size() - 1];

            if (tmpLastEndTimestamp - fullTextEndTimestamp > maxDiff) {
                modLastText.timestamps.pop_back();
                modLastText.words.pop_back();   // added on 5.10.2025
            }
            else
                break;
        }

        int shift = 0;
        int maxShift = fullText.timestamps.size(); //std::min(fullText.timestamps.size(), modLastText.timestamps.size());    // leave it this way for now

        int optimalShift = shift;
        float optimalDTW = std::numeric_limits<float>::max();

        while (shift <= maxShift) {
            NeMoInference::WordLevelOutput tmpFull = fullText;
            NeMoInference::WordLevelOutput tmpLast = modLastText; //lastText;

            // leave only last "shift" entries in tmpFull
            tmpFull.timestamps.erase(tmpFull.timestamps.begin(), tmpFull.timestamps.end() - shift);
            tmpFull.words.erase(tmpFull.words.begin(), tmpFull.words.end() - shift); // added on 5.10.2025
            // leave only first "shift" entries in tmpLast
            if (shift < tmpLast.timestamps.size()) {
                tmpLast.timestamps.erase(tmpLast.timestamps.begin() + shift, tmpLast.timestamps.end());
                tmpLast.words.erase(tmpLast.words.begin() + shift, tmpLast.words.end());    // added on 5.10.2025
            }

            // break if first timestamp is less than zero
            if (tmpFull.timestamps.size() && tmpFull.timestamps[0] < 0.0f)
                break;

            // COMMENTED ON 5.10.2025
            //float dtwDist = DTW(tmpFull.timestamps, tmpLast.timestamps);
            //if (dtwDist < optimalDTW) {
            //    optimalDTW = dtwDist;
            //    optimalShift = shift;
            //}

            // ADDED ON 5.10.2025 (DTW times Levenshtein distance as a metric)
            // MODIFIED ON 12.3.2026 (DTW timestamp differences as a metric! Because first timestamp isn't always correct, leading to larger errors when computing DTW's directly)
            float dtwDist = DTW(tmpFull.timestamps, tmpLast.timestamps);
            float levDist = levenshteinDist(tmpFull.toString(), tmpLast.toString());
            //wcout << "tmpFull: " << tmpFull.toString() << endl;
            //wcout << "tmpLast: " << tmpLast.toString() << endl;
            //wcout << "levDist: " << levDist << endl << endl;
            float mulDist = levDist * dtwDist; //dtwDist;    // dtwDist * (float)levDist
            if (mulDist < optimalDTW) {
                optimalDTW = mulDist;
                optimalShift = shift;
            }

            //wprintf(L"tmpFull timestamps: %s\ntmpLast timestamps: %s\n", tmpFull.toStringTimestamps().c_str(), tmpLast.toStringTimestamps().c_str());
            //wprintf(L"dtwDist: %.2f\n", dtwDist);
            //getchar();
            //wcout << "fullsize: " << tmpFull.timestamps.size() << "; lastsize: " << tmpLast.timestamps.size() << endl;
            shift++;
        }
        //wprintf(L"optimalShift = %i, optimalDTW = %.2f\n", optimalShift, optimalDTW);

        // TEST CODE START  (12.3.2026)
        /*vector<int> newLineIndexes;
        for (int i = 0; i < (int)fullTextOut.words.size(); i++) {
            wstring word = fullTextOut.words[i];
            if (word.length() && word.back() == L'*')
                newLineIndexes.push_back(i);
        }*/

        vector<float> newLineTimes; // probably word's *end*time would be better
        for (int i = 0; i < (int)fullTextOut.words.size(); i++) {
            wstring word = fullTextOut.words[i];
            if (word.length() && word.back() == L'*')
                newLineTimes.push_back(fullTextOut.timestamps[i]);
        }
        // TEST CODE END

        // erase last "optimalShift" entries
        fullTextOut.timestamps.erase(fullTextOut.timestamps.end() - optimalShift, fullTextOut.timestamps.end());
        fullTextOut.words.erase(fullTextOut.words.end() - optimalShift, fullTextOut.words.end());

        // insert "lastText" to end of "fullTextOut"
        fullTextOut.timestamps.insert(fullTextOut.timestamps.end(), lastText.timestamps.begin(), lastText.timestamps.end());
        fullTextOut.words.insert(fullTextOut.words.end(), lastText.words.begin(), lastText.words.end());

        for (float time : newLineTimes) {
            int closestTimeIndex = 0;
            float closestDiff = std::numeric_limits<float>::max();
            for (int idx = 0; idx < fullTextOut.timestamps.size(); idx++) {
                float diff = abs(fullTextOut.timestamps[idx] - time);
                if (diff < closestDiff) {
                    closestDiff = diff;
                    closestTimeIndex = idx;
                }
            }

            if (closestDiff <= 0.1f) {
                wstring& word = fullTextOut.words[closestTimeIndex];
                if (word.length() && word.back() != L'*')
                    word.push_back(L'*');
            }
        }

        return fullTextOut;
    }

    bool isAudioAllSilence(vector<float>& audioChunk) {
        bool silent = true;

        for (int i = 0; i < audioChunk.size(); i++) {
            if (audioChunk[i] != 0.0f) {
                silent = false;
                break;
            }
        }

        return silent;
    }

    vector<bool> dilate(vector<bool> v, int numDilate) {
        vector<bool> vOut = v;
        for (int i = 0; i < v.size(); i++)
        {
            int from = std::max(i - numDilate, 0);
            int to = std::min(i + numDilate, (int)v.size() - 1);
            if (v[i] == true) {
                for (int j = from; j <= to; j++)
                    vOut[j] = true;
            }
        }

        return vOut;
    }

    bool isInferenceReady() {
        return bInferenceReady;
    }

    void setInferenceReady(bool ready) {
        bInferenceReady = ready;
    }

#if defined(__ANDROID__)
    void inferenceLoopASR(string modelPath, string vocabPath, void (*fTextCallback)(wstring addedTextUTF8), shared_ptr<MainStream> stream, bool installTime) {
#elif defined(_WIN32)
        void inferenceLoopASR(wstring modelPath, wstring vocabPath, void (*fTextCallback)(wstring addedTextUTF8), bool micOrStream) {
#endif
        float lastSilenceTime = 0.0f;
        bool prevSilence = true;	// TODO mismatch: on windows it was "false"

        shared_ptr<SimpleSpeakerChangeDetection> spkChgDetector;
        if(experimental) {
#if defined(__ANDROID__)
            spkChgDetector = make_shared<SimpleSpeakerChangeDetection>(assetManager, captureWindowSeconds, audioPaddingTime);
#elif defined(_WIN32)
            spkChgDetector = make_shared<SimpleSpeakerChangeDetection>(captureWindowSeconds, audioPaddingTime);
#endif
        }

#if defined(__ANDROID__)
        NeMoInference model = NeMoInference(assetManager, modelPath, vocabPath, unspacedText, suppressTokens, installTime);
#elif defined(_WIN32)
        NeMoInference model = NeMoInference(modelPath, vocabPath, false, unspacedText, suppressTokens);
#endif

#if defined(_WIN32)
        shared_ptr<AudioStream> stream = make_shared<AudioStream>(micOrStream);
#endif

        int initialFullAudioLength = (int)(16000.0f * captureWindowSeconds);
        vector<float> fullAudio(initialFullAudioLength, 0.0f);
        NeMoInference::WordLevelOutput wlFullText = NeMoInference::WordLevelOutput(unspacedText);

        setInferenceReady(true);

#if defined(_WIN32)
        fTextCallback(L"");
#endif

        auto timer0 = chrono::high_resolution_clock::now();
        while (stopLooperThread == false) {
#if defined(__ANDROID__)
            if(resetAudioText) {
                fullAudio = vector<float>(initialFullAudioLength, 0.0f);
                wlFullText = NeMoInference::WordLevelOutput(unspacedText);
                stream->setText(L"");
                resetAudioText = false;
            }
#endif

            if(pauseLooperThread == true) {
#if defined(__ANDROID__)
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[inferenceLoopASR] std::this_thread::sleep_for");
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                timer0 = chrono::high_resolution_clock::now();
                continue;
            }

            vector<float> audioChunk = stream->getNextAudioChunk(captureStepSeconds);
            if (!audioChunk.size()) {
                //wprintf(L"[inferenceLoopASR] zero size, continuing\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            if (isAudioAllSilence(audioChunk)) { // don't run model inference if all we have is silent audio
#if defined(_WIN32)
                wprintf(L"[inferenceLoopASR] audio is silent, sleeping\n");
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                timer0 = chrono::high_resolution_clock::now();
                continue;
            }

            int vadChunkSize = (int)(16000.0f * 0.020f);
            vector<bool> vadOut = nemoVad.forward(audioChunk, true);

            int numKeep = (int)ceil((float)audioChunk.size() / (float)vadChunkSize);
            if(numKeep < vadOut.size()) // if for some reason audio chunk (about 0.6s) is longer than vad buffer (5s)
                vadOut.erase(vadOut.begin(), vadOut.end() - numKeep);

            float vadLastSilenceTime = 0.0f;
            for (int i = vadOut.size() - 1; i >= 0; i--) {
                if (vadOut[i] == true)
                    break;
                vadLastSilenceTime += 0.020f;
            }

            int numPrevCopy = 0;
            static vector<bool> vadOutPrev;    // previous vad out (non-dilated)
            static vector<bool> vadOutPrevDilated;    // previous vad out (dilated)
            vector<bool> vadOutDilated;
            vadOutDilated.insert(vadOutDilated.end(), vadOutPrev.begin(), vadOutPrev.end());
            vadOutDilated.insert(vadOutDilated.end(), vadOut.begin(), vadOut.end());
            vadOutDilated = dilate(vadOutDilated, 30);
            for(int i = (int)vadOutPrevDilated.size() - 1; i >= 0; i--) {
                if(vadOutPrevDilated[i] == 1)
                    break;
                if(vadOutDilated[i] == 0)
                    break;
                numPrevCopy++;
            }
            vadOutPrevDilated = vector<bool>(vadOutDilated.begin(), vadOutDilated.begin() + vadOutPrev.size());
            vadOutDilated.erase(vadOutDilated.begin(), vadOutDilated.begin() + vadOutPrev.size());
            vadOutPrev = vadOut;
            vadOut = vadOutDilated;

            vector<float> prevFullAudio = fullAudio;
            vector<float> newAudio;

            static vector<float> prevAudioChunk((int)(captureStepSeconds * 16000.0f));
            {
                int from = std::max((int)prevAudioChunk.size() - numPrevCopy*vadChunkSize, 0);
                int to = (int)prevAudioChunk.size();
                newAudio.insert(newAudio.end(), prevAudioChunk.begin() + from, prevAudioChunk.begin() + to);
            }

            for (int i = 0; i < vadOut.size(); i++) {
                if (vadOut[i] == false)
                    continue;

                int from = i * vadChunkSize;
                int to = std::min((int)audioChunk.size(), from + vadChunkSize);

                newAudio.insert(newAudio.end(), audioChunk.begin() + from, audioChunk.begin() + to);
            }
            prevAudioChunk = audioChunk;
            fullAudio.insert(fullAudio.end(), newAudio.begin(), newAudio.end());

            int fullAudioNewSamples = (int)(fullAudio.size() - prevFullAudio.size());
            if (fullAudioNewSamples == 0) {
                float timeDiff = (float)audioChunk.size() / 16000.0f;

                // append newline character if lastSilenceTime >= enduringSilenceCount (2 seconds)
                lastSilenceTime += timeDiff;
                if (lastSilenceTime >= enduringSilenceCount) {
                    if (!wlFullText.words.empty()) {
                        wstring& lastWord = wlFullText.words.back();
                        if (lastWord.back() != L'*') {
#if defined(__ANDROID__)
                            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "adding newline!");
#endif
                            lastWord.push_back(L'*');
                        }
                    }
                }
                // (end of append code)

                if (prevSilence == false) {
#if defined(__ANDROID__)
                    __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "decoding last word, though no new speech in this chunk");
#endif
                    //wprintf(L"decoding last word, though no new speech in this chunk\n");
                    prevSilence = true;
                }
                else {
#if defined(__ANDROID__)
                    __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "continuing, no new speech in this chunk");
#endif
                    fTextCallback(wlFullText.toString(false)); // print, but now include last word (false param)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    timer0 = chrono::high_resolution_clock::now(); // since it was silence, we reset "audio start"-count, i.e. timer0
                    continue;
                }
            }
            else {
                lastSilenceTime = 0.0f;
                prevSilence = false;
            }
            float fullAudioNewTime = (float)fullAudioNewSamples / 16000.0f;

            // erase from beginning of fullAudio, to keep it captureWindowSeconds
            int diff = (int)fullAudio.size() - (int)(captureWindowSeconds * 16000.0f);
            float timeDiff = (float)diff / 16000.0f;
            if (diff > 0)
                fullAudio.erase(fullAudio.begin(), fullAudio.begin() + diff);

            // time shift previous timestamps
            if (timeDiff > 0.0f) {
                for (float& ts : wlFullText.timestamps)
                    ts = ts - timeDiff;

                //for (float& ts : spkChgTimestamps)
                //    ts = ts - timeDiff;
            }

            // ASR inference
            vector<float> fullAudioPadded = fullAudio;
            int numPadding = (int)round(audioPaddingTime * 16000.0f);    // 8000 for 0.5f audioPaddingTime
            fullAudioPadded.insert(fullAudioPadded.begin(), numPadding, 0.0f);
            fullAudioPadded.insert(fullAudioPadded.end(), numPadding, 0.0f);

            auto t0 = chrono::high_resolution_clock::now();
            NeMoInference::NeMoOut modelOut = model.forward(fullAudioPadded);
            auto t1 = chrono::high_resolution_clock::now();

#if defined(__ANDROID__)
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "model.forward took %lld ms", chrono::duration_cast<chrono::milliseconds>(t1-t0).count());
#endif

            NeMoInference::WordLevelOutput wlOut = model.convertTokensToWords(modelOut);

#if defined(__ANDROID__)
            //__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "wlOut: %S", wlOut.toString(false).c_str());
#elif defined(_WIN32)
            // DEBUG OUTPUT
            //wcout << L"wlOut = \"" << wlOut.toString() << L"\"" << endl;
            //wcout << L"wlOut timestamps:\n" << wlOut.toStringTimestamps() << endl;
            //for (int i = 0; i < modelOut.rawOutTokens.size(); i++) {
            //    wprintf(L"%i ", modelOut.rawOutTokens[i]);
            //}
            //wprintf(L"\n");
#endif

            /*float vadFirstSilenceTime = audioPaddingTime;
            vector<bool> vadOutFullAudio;
            if(fullAudio.size())
                vadOutFullAudio = nemoVad.forward(fullAudio, false);

            for (int i = 0; i < vadOutFullAudio.size(); i++) {
                if (vadOutFullAudio[i] == true)
                    break;
                vadFirstSilenceTime += 0.020f;
            }
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[NEMO_REALFEED] firstSilenceTime = %.2f\n", vadFirstSilenceTime);
             */

            NeMoInference::WordLevelOutput wlMod = NeMoInference::WordLevelOutput(unspacedText);

            bool correctTimestampBias = isConformer;
            if (correctTimestampBias) {
                //float firstTimestamp = wlOut.timestamps.size() ? wlOut.timestamps[0] : 0.0f;
                for (int i = 0; i < wlOut.timestamps.size(); i++) {
                    //if (i == 0)
                    //    wlOut.timestamps[i] = vadFirstSilenceTime;
                    //else {
                    wlOut.timestamps[i] += timeBias;
                    //}
                }
#if defined(_WIN32)
                //wcout << L"wlOut MODIFIED timestamps:\n" << wlOut.toStringTimestamps() << endl;
#endif
            }

            for (int i = 0; i < wlOut.timestamps.size(); i++) {
                wlMod.timestamps.push_back(wlOut.timestamps[i]);
                wlMod.words.push_back(wlOut.words[i]);
            }

            // delete all words "overlapTextWindow" seconds before old text's last timestamp
            float lastFullTextTimestamp = 0.0f;
            if (wlFullText.timestamps.size())
                lastFullTextTimestamp = wlFullText.timestamps[wlFullText.timestamps.size() - 1];

            int wlModNumDelete = 0;
            int numPastWords = 0;

            int maxPastWordsToKeep = 3;

            for (int i = 0; i < wlMod.words.size(); i++) {
                if (wlMod.timestamps[i] < (lastFullTextTimestamp - overlapTextWindow))
                    wlModNumDelete = i;

                if (wlMod.timestamps[i] < lastFullTextTimestamp)
                    numPastWords = i;
            }
            wlModNumDelete = std::max(wlModNumDelete, std::max(numPastWords - maxPastWordsToKeep, 0));

            if (wlModNumDelete > 0) {
                wlMod.words.erase(wlMod.words.begin(), wlMod.words.begin() + wlModNumDelete);
                wlMod.timestamps.erase(wlMod.timestamps.begin(), wlMod.timestamps.begin() + wlModNumDelete);
            }

            // If last word is cut in-between speech (last silence time is low), remove it
            bool removeLast = false;
            if (removeUnfinishedWord && vadLastSilenceTime < 0.1f && !wlMod.words.empty() && prevSilence == false) {
#if defined(__ANDROID__)
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "Removing last word. lastSilenceTime = %.2f", modelOut.lastSilenceTime);
#elif defined(_WIN32)
                wprintf(L"Removing last word. lastSilenceTime = %.2f\n", vadLastSilenceTime);//modelOut.lastSilenceTime);
#endif
                removeLast = true;
            }

            /*__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING]");
            for (int i = 0; i < wlFullText.words.size(); i++)
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING]\twlFullText[%i] @t=%.3f: %S", i, wlFullText.timestamps[i], wlFullText.words[i].c_str());
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING] WITH:");
            for (int i = 0; i < wlMod.words.size(); i++)
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING]\twlMod[%i] @t=%.3f: %S", i, wlMod.timestamps[i], wlMod.words[i].c_str());
            */

            /*
            wprintf(L"ALIGNING:\n");
            for (int i = 0; i < wlFullText.words.size(); i++)
                wprintf(L"\twlFullText[%i] @t=%.3f: %s\n", i, wlFullText.timestamps[i], wlFullText.words[i].c_str());
            wprintf(L"WITH:\n");
            for (int i = 0; i < wlMod.words.size(); i++)
                wprintf(L"\twlMod[%i] @t=%.3f: %s\n", i, wlMod.timestamps[i], wlMod.words[i].c_str());
            */

            wlFullText = align(wlFullText, wlMod);

            /*wprintf(L"RESULT:\n");
            for (int i = 0; i < wlFullText.words.size(); i++)
                wprintf(L"\twlFullText_RES[%i] @t=%.3f: %s\n", i, wlFullText.timestamps[i], wlFullText.words[i].c_str());
            wprintf(L"\n");*/
            /*__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING] RESULT:");
            for (int i = 0; i < wlFullText.words.size(); i++)
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[ALIGNING] \twlFullText_RES[%i] @t=%.3f: %S\n", i, wlFullText.timestamps[i], wlFullText.words[i].c_str());
            */

            // ##################  SPEAKER CHANGE DETECTION ####################
            //detectSpeakerChange(fullAudioPadded, newAudio, spkChgTimestamps, wlFullText);
            if(isConformer && experimental && spkChgDetector != nullptr)
                spkChgDetector->updateState(wlFullText, audioChunk, fullAudioPadded, timeDiff);
            // #################################################################

            auto timer1 = chrono::high_resolution_clock::now();
            float holdupTime = 0.0f;   // assures smoother letter-by-letter printout experience during continuous speech
            float elapsedSeconds = holdupTime + 0.001f * (float)chrono::duration_cast<chrono::milliseconds>(timer1 - timer0).count();
#if defined(__ANDROID__)
            stream->setTimeASR(elapsedSeconds);
#elif defined(_WIN32)
            setTimeASR(elapsedSeconds);
#endif
            timer0 = chrono::high_resolution_clock::now();

            //__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "full text: %S", wlFullText.toString(removeLast).c_str());

            fTextCallback(wlFullText.toString(removeLast));
        }

        fTextCallback(L"");

        setInferenceReady(false);
        looperThread.detach();
#if defined(_WIN32)
        wprintf(L"Out of loop\n");
#endif
    }
};

#endif
