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
#include <android/log.h>
#include "logmel_nemo.h"
#include "nemo_inference.h"
#include "vad.h"
#include "nemo_vad.h"
#include "nemo_sortformer.h"
#include "stream.h"
#include "levenshtein.h"
#include "fvad.h"   // https://github.com/dpirch/libfvad/tree/master
#include "spkchg.h"
#include "dtw.h"

class NeMoRealFeed {
public:
    atomic<bool> bInferenceReady = { false };
    float captureStepSeconds = 0.6f; //0.75f;    // every captureStepSeconds, obtain new audio
    float captureWindowSeconds = 5.0f;//5.0f; //6.0f; // speech recognition audio buffer size
    float enduringSilenceCount = 2.0f;  // in TranscribeActivity, this many seconds need to be silent to print empty line
    float overlapTextWindow = 0.5f; // 2.0f // longest segment length that will be aligned (larger value corrects more text from past)

    float audioPaddingTime = 0.5f;   // how much audio will be padded on left and right

    bool removeUnfinishedWord = false;   // don't guess last word
    //float compressFactor = 1.0f;    // Experimental! better to set to 1.0

    bool unspacedText;  // set in constructor (true for Chinese and Japanese)
    float timeBias; // set in constructor
    bool isConformer; // set in constructor
    bool suppressTokens; // set in constructor
    bool experimental; // set in constructor

    atomic<bool> stopLooperThread{ false };
    atomic<bool> pauseLooperThread{ false };
	thread looperThread;

    AAssetManager *assetManager;
    //SileroVAD vad;
    NeMoVAD nemoVad;

    //NeMoRealFeed(AAssetManager *manager): model(manager, "v2.onnx"), vad(manager, "silero_vad_16k_op15_NEW.onnx") {
    NeMoRealFeed(AAssetManager *manager): assetManager(manager), /*vad(manager, "silero_vad_16k_op15_NEW.onnx")*/nemoVad(manager, "marblenet2.onnx", 5.0f) {
        /*
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "reading wave asset");
        vector<float> audio = readWaveAsset(manager, "test2.wav");
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "calling forward");
        auto t0 = chrono::high_resolution_clock::now();
        NeMoInference::NeMoOut out = model.forward(audio);
        auto t1 = chrono::high_resolution_clock::now();
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "FIRST model.forward took %i ms", (int)chrono::duration_cast<chrono::milliseconds>(t1-t0).count());
        NeMoInference::WordLevelOutput wlOut = model.convertTokensToWords(out);
        string word;
        if(!wlOut.words.empty())
            word = string(wlOut.words[0].begin(), wlOut.words[0].end());
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "Got %i words. First one is %s", (int)wlOut.words.size(), word.c_str());
        */
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

	bool NeMoBeginFeed(string modelPath, string vocabPath, void (*f)(wstring fullText), shared_ptr<MainStream> stream, float timeBias=0.0f, bool unspacedText=false, bool punctuate=false, bool isConformer=false, bool installTime=true, bool suppressTokens=false, bool experimental=false) {
        if (looperThread.joinable()) {
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "joinable = true, returning false");
            return false;	// signalize that thread is still running, and that NeMoStopFeed should be called first
        }

        setInferenceReady(false);

        this->unspacedText = unspacedText;
        this->timeBias = timeBias;
        // this->punctuate = punctuate;
        this->isConformer = isConformer;
        this->suppressTokens = suppressTokens;
        this->experimental = experimental;

        stopLooperThread = false;
		//looperThread = thread(&NeMoRealFeed::inferenceLoopASR, this, f, stream);
        looperThread = thread(&NeMoRealFeed::inferenceLoopASR, this, modelPath, vocabPath, f, stream, installTime);
		return true;
	}

	void NeMoStopFeed() {
		stopLooperThread = true;
	}

    void NeMoPauseResume(bool pause) {
        pauseLooperThread = pause;
    }

    vector<float> maxNormalize(vector<float> &audio) {
        vector<float> out;

        if (!audio.size())
            return out;

        float maxValue = audio[0];
        for (int i = 0; i < audio.size(); i++) {
            if (audio[i] > maxValue)
                maxValue = audio[i];
        }

        for (int i = 0; i < audio.size(); i++)
            out.push_back(audio[i] / maxValue);

        return out;
    }

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
            wcout << L"INFO: lastText was shorter than fullText!" << endl;
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
    /*
     * COMMENTED ON 3.8.2026
     * NeMoInference::WordLevelOutput align(NeMoInference::WordLevelOutput fullText, NeMoInference::WordLevelOutput lastText)
    {
        NeMoInference::WordLevelOutput fullTextOut = fullText;
        NeMoInference::WordLevelOutput modLastText = lastText;

        if (lastText.timestamps.size() && fullText.timestamps.size() &&
            lastText.timestamps.back() < fullText.timestamps.back()) {
            //wcout << L"NOT ALIGNING: lastText is shorter than fullText!" << endl;
            //wcout << L"Last text timestamps: " << lastText.toStringTimestamps() << endl;
            //wcout << L"Full text timestamps: " << fullText.toStringTimestamps() << endl;
            //return fullText;
        }

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
            float dtwDist = DTW(tmpFull.timestamps, tmpLast.timestamps); //deltaDTW(tmpFull.timestamps, tmpLast.timestamps);
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

        vector<float> newLineTimes; // probably word's *end*time would be better
        for (int i = 0; i < (int)fullTextOut.words.size(); i++) {
            wstring word = fullTextOut.words[i];
            if (word.length() && word.back() == L'*')
                newLineTimes.push_back(fullTextOut.timestamps[i]);
        }

        // erase last "optimalShift" entries
        fullTextOut.timestamps.erase(fullTextOut.timestamps.end() - optimalShift, fullTextOut.timestamps.end());
        fullTextOut.words.erase(fullTextOut.words.end() - optimalShift, fullTextOut.words.end());

        // insert "lastText" to end of "fullTextOut"
        fullTextOut.timestamps.insert(fullTextOut.timestamps.end(), lastText.timestamps.begin(), lastText.timestamps.end());
        fullTextOut.words.insert(fullTextOut.words.end(), lastText.words.begin(), lastText.words.end());

        for (float time: newLineTimes) {
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
                wstring &word = fullTextOut.words[closestTimeIndex];
                if (word.length() && word.back() != L'*')
                    word.push_back(L'*');
            }
        }

        return fullTextOut;
    }*/

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

	void inferenceLoopASR(string modelPath, string vocabPath, void (*fTextCallback)(wstring addedTextUTF8), shared_ptr<MainStream> stream, bool installTime) {
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[inferenceLoopASR] start loop");
        vector<float> fullAudio = vector<float>((int)(16000.0f * captureWindowSeconds), 0.0f);
        //NeMoInference::WordLevelOutput wlFullText;
        NeMoInference::WordLevelOutput wlFullText = NeMoInference::WordLevelOutput(unspacedText);
        vector<float> spkChgTimestamps;

        float lastSilenceTime = 0.0f;
        bool prevSilence = true;

        shared_ptr<SimpleSpeakerChangeDetection> spkChgDetector;
        if(experimental)
            spkChgDetector = make_shared<SimpleSpeakerChangeDetection>(assetManager, captureWindowSeconds, audioPaddingTime);

        NeMoInference model = NeMoInference(assetManager, modelPath, vocabPath, unspacedText, suppressTokens, installTime);

        setInferenceReady(true);

        auto timer0 = chrono::high_resolution_clock::now();
        while (stopLooperThread == false) {
            if(pauseLooperThread == true) {
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "[inferenceLoopASR] std::this_thread::sleep_for");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            vector<float> audioChunk = stream->getNextAudioChunk(captureStepSeconds);
            if (!audioChunk.size()) {
                //wprintf(L"[inferenceLoopASR] zero size, continuing\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "vad_out=%i", (int)vadOut[0]);

            vector<float> prevFullAudio = fullAudio;
            vector<float> newAudio;

            static vector<float> prevAudioChunk((int)(captureStepSeconds * 16000.0f));
            {
                // numPrevCopy = std::min(numPrevCopy, 6);
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "numPrevCopy=%i", numPrevCopy);
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
                            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "adding newline!");
                            lastWord.push_back(L'*');
                        }
                    }
                }
                // (end of append code)

                if (prevSilence == false) {
                    __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "decoding last word, though no new speech in this chunk");
                    //wprintf(L"decoding last word, though no new speech in this chunk\n");
                    prevSilence = true;
                }
                else {
                    __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "continuing, no new speech in this chunk");
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
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "model.forward took %lld ms", chrono::duration_cast<chrono::milliseconds>(t1-t0).count());

            NeMoInference::WordLevelOutput wlOut = model.convertTokensToWords(modelOut);
            //__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "wlOut: %S", wlOut.toString(false).c_str());

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
                wcout << L"wlOut MODIFIED timestamps:\n" << wlOut.toStringTimestamps() << endl;
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
                __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "Removing last word. lastSilenceTime = %.2f", modelOut.lastSilenceTime);
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
                spkChgDetector->updateState(wlFullText, audioChunk, fullAudioPadded, timeDiff/*, vadFirstSilenceTime*/);
            // #################################################################

            auto timer1 = chrono::high_resolution_clock::now();
            float holdupTime = 0.0f;   // assures smoother letter-by-letter printout experience during continuous speech
            float elapsedSeconds = holdupTime + 0.001f * (float)chrono::duration_cast<chrono::milliseconds>(timer1 - timer0).count();
            stream->setTimeASR(elapsedSeconds);
            timer0 = chrono::high_resolution_clock::now();

            //__android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "full text: %S", wlFullText.toString(removeLast).c_str());

            fTextCallback(wlFullText.toString(removeLast));
		}

        setInferenceReady(false);
		looperThread.detach();
	}
};

#endif
