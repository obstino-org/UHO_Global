// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <string>

using namespace std;

class MainStream {
public:
    mutex textLock;
    mutex newAudioLock;

    int vadChunkSize = 320; // 320==20ms //512;
    vector<float> audio;

    wstring fullText;
    int fullTextCutIndex = 0;   // (index before which text is removed)
    wstring truncFullText;

    chrono::duration<
            long long int,
            std::ratio<std::chrono::high_resolution_clock::duration::period::num,
                std::chrono::high_resolution_clock::duration::period::den>>
        lastUpdateTime = std::chrono::system_clock::now().time_since_epoch();

    wstring getText() {
        wstring textOut;

        textLock.lock();
        textOut = fullText;
        //text = "";
        textLock.unlock();

        return textOut;
    }

    void setText(wstring newFullText) {
        //textLock.lock();
        //text += _text;
        //textLock.unlock();
        if (newFullText.length() == 0) {
            textLock.lock();
            fullText = L"";
            truncFullText = L"";
            lastUpdateTime = std::chrono::system_clock::now().time_since_epoch();
            textLock.unlock();
            return;
        }

        //newFullText = correctWstringMapping(newFullText);

        textLock.lock();
        fullText = newFullText;
        if(fullText.length() && fullTextCutIndex < fullText.length())
            truncFullText = fullText.substr(fullTextCutIndex);
        lastUpdateTime = std::chrono::system_clock::now().time_since_epoch();
        textLock.unlock();
    }

    void addNewAudio(vector <float>& _audio) {
        newAudioLock.lock();
        audio.insert(audio.end(), _audio.begin(), _audio.end());
        newAudioLock.unlock();
    }

    vector<float> getNextAudioChunk(float audioChunkSeconds = 2.0) {
        float sr = 16000.0f;
        int desiredSize = (int)(sr * audioChunkSeconds);
        static vector<float> out;   // (12.8.2025 - changed to static)

        newAudioLock.lock();
        // if number of samples is less than audioChunkSeconds, return empty buffer
        if (audio.size() < desiredSize) {
            newAudioLock.unlock();
            return vector<float>();
        }
        // get all available samples
        out.insert(out.end(), audio.begin(), audio.end());
        audio.clear();
        newAudioLock.unlock();

        // Added functionality to truncate 'out', so that it will be divisible by vadChunkSize
        // if it wasn't divisible, leave remaining samples for next iteration of getNextAudioChunk
        int diff = out.size() % vadChunkSize;
        vector<float> actualOut = vector<float>(out.begin(), out.end() - diff);
        out.erase(out.begin(), out.end() - diff);

        return actualOut;
    }

    atomic<float> timeASR;

    float getTimeASR() {
        return timeASR;
    }

    void setTimeASR(float seconds) {
        timeASR = seconds;
    }

    MainStream(): timeASR(0.0f) { }
};

