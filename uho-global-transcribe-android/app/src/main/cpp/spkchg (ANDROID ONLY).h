//
// Created by bko on 25/05/2026.
//

#ifndef UHO_GLOBAL_TRANSCRIBE_SPKCHG_H
#define UHO_GLOBAL_TRANSCRIBE_SPKCHG_H

#include <iostream>
#include <vector>
#include <set>
#include <stdio.h>
#include "nemo_sortformer.h"
#include "nemo_inference.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

class SimpleSpeakerChangeDetection {
public:
    NeMoSortformer sortformer;
    vector<float> spkChgTimestamps;
    float captureWindowSeconds;
    float audioPaddingTime;
    float modelBias;

    SimpleSpeakerChangeDetection(AAssetManager *manager, float captureWindowSeconds, float audioPaddingTime):
        sortformer(manager, "sf21.int8.shrink.onnx", 10.00f /*4.96f*/)
    {
        this->audioPaddingTime = audioPaddingTime;
        this->captureWindowSeconds = captureWindowSeconds;
    }

    void updateState(NeMoInference::WordLevelOutput& wlFullText, vector<float>& audioChunk, vector<float>& fullAudioPadded, float timeDiff/*, float firstSilenceTime*/) {
        auto t0 = chrono::high_resolution_clock::now();

        for (float& ts : spkChgTimestamps)
            ts = ts - timeDiff;

        //if using HybridSpeakerChangeDetection, uncomment below
        //auto sfOut = sortformer.forward2(audioChunk, true);
        //vector<float> spkChgOut = sfOut.first;
        vector<float> spkChgOut = sortformer.forward2(audioChunk);

        //vector<bool> sfVAD = sfOut.second;
        float normFullAudioSeconds = (float)fullAudioPadded.size() / 16000.0f;
        float audioChunkSeconds = (float)audioChunk.size() / 16000.0f;
        addSpeakerChangeTextIndicators(spkChgOut, wlFullText, normFullAudioSeconds, audioChunkSeconds, audioPaddingTime);

        auto t1 = chrono::high_resolution_clock::now();
        wprintf(L"%i ms for full part of updateState\n", (int)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());
    }

    void addSpeakerChangeTextIndicators(vector<float>& spkChgOut, NeMoInference::WordLevelOutput& wlFullText, float normFullAudioSeconds, float audioChunkSeconds, float audioPaddingTime) {
        // time shift and append current timestamps, so that they can be aligned with timestamps of words
        for (float& time : spkChgOut) {
            time = normFullAudioSeconds - audioChunkSeconds + time - audioPaddingTime;
            float optTime = time;
            wprintf(L"spkChgOut = %.3f, however its optTime = %.3f\n", time, optTime);
            spkChgTimestamps.push_back(optTime);
        }

        // erase timestamps that aren't relevant anymore (negative times)
        int spkChgRemoveEndIndex = -1;
        for (int i = 0; i < spkChgTimestamps.size(); i++) {
            if (spkChgTimestamps[i] >= 0.0f) {
                spkChgRemoveEndIndex = i;
                break;
            }
        }

        if (spkChgRemoveEndIndex >= 0)
            spkChgTimestamps.erase(spkChgTimestamps.begin(), spkChgTimestamps.begin() + spkChgRemoveEndIndex);

        set<int> closestIndexes;
        for (int i = 0; i < spkChgTimestamps.size(); i++) {
            float spkChgTime = spkChgTimestamps[i];

            // find best alignment of current speaker change time (spkChgTime) with timestamps
            int closestIndex = -1;
            float closestDiff = std::numeric_limits<float>::max();
            for (int j = 0; j < wlFullText.timestamps.size(); j++) {
                float diff = wlFullText.timestamps[j] - spkChgTime; // in code design, spkChgTime precedes or is equal to wlFullText's timestamp (e.g. 8.48 precedes 8.50)
                float absDiff = abs(diff);
                if (diff < closestDiff && diff >= 0) {
                    //wprintf(L"wlFullText.timestamp[j=%i] = %.3f, spkChgTime(opt) = %.3f", j, wlFullText.timestamps[j], spkChgTime);
                    //getchar();
                    closestDiff = diff;
                    closestIndex = j;
                }
            }

            // append "em dash" symbol if speaker change was detected
            //if (closestIndex >= 0 && closestIndex < ((int)wlFullText.timestamps.size() - 1) && closestIndexes.count(closestIndex) == 0) {
            if (closestIndex >= 0 && closestIndexes.count(closestIndex) == 0) {
                closestIndexes.insert(closestIndex);

                //wstring emDash = L"\u2014";
                //if (wlFullText.words[closestIndex].size() && wlFullText.words[closestIndex][0] != emDash[0])
                //    wlFullText.words[closestIndex] = emDash + wlFullText.words[closestIndex];

                if (std::find(wlFullText.spkChgIndexes.begin(), wlFullText.spkChgIndexes.end(), closestIndex) == wlFullText.spkChgIndexes.end())
                    wlFullText.spkChgIndexes.push_back(closestIndex);

                // COMMENTED ON 7.5.2026, REPLACED BY ABOVE
                //wstring strSpkChg = L">> ";
                //if (wlFullText.words[closestIndex].size() &&
                //    wlFullText.words[closestIndex].find(strSpkChg) != 0)
                //    wlFullText.words[closestIndex] = strSpkChg + wlFullText.words[closestIndex];
            }
        }
    }
};

#endif //UHO_GLOBAL_TRANSCRIBE_SPKCHG_H
