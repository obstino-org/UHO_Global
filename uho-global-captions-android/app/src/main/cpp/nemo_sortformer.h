// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <memory>

#if defined(__ANDROID__)
#include "onnxruntime/onnxruntime_cxx_api.h"
#elif defined(_WIN32)
#include "onnxruntime_cxx_api.h"
#endif

#include "logmel_nemo.h"
using namespace std;

class NeMoSortformer {
public:
    // config
    int numMelFilters = 128;
    int sampleRate = 16000;
    float timeUnitSortformer = 0.080;   // 80 ms
    float minProb = 0.35f;  // minimum probability (out of 4 speakers) of speech onset

    float memoryWindow;
    vector<float> audioBuffer;
    int maxBuffer;

    LogMelNeMo melCompute;

    Ort::Env env;
    Ort::SessionOptions options;
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    unique_ptr<Ort::Session> model;

#if defined(__ANDROID__)
    NeMoSortformer(AAssetManager* manager, string modelPath = "sf21.int8.shrink.onnx", float memoryWindow = 5.0f) :
        memoryWindow(memoryWindow), melCompute(manager, "melfilters128.bin", numMelFilters)
#elif defined(_WIN32)
    NeMoSortformer(wstring modelPath = L"sf21.int8.shrink.onnx", float memoryWindow = 5.0f) :
        memoryWindow(memoryWindow), melCompute(L"melfilters128.bin", numMelFilters)
#endif
    {
        try {
            options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, "nemo_sortformer_inference");
            //model = make_unique<Ort::Session>(env, modelPath.c_str(), options);

#if defined(__ANDROID__)
            vector<uint8_t> modelData = readAsset(manager, (char*)modelPath.c_str());
#elif defined(_WIN32)
            vector<uint8_t> modelData = readFile(string(modelPath.begin(), modelPath.end()));
#endif

            model = make_unique<Ort::Session>(Ort::Session(env, modelData.data(), modelData.size(), options));
        }
        catch (const Ort::Exception& e) {
            std::cerr << "Error creating session: " << e.what() << std::endl;
        }

        maxBuffer = (int)((float)sampleRate * memoryWindow);

        resetAudio();
    }

    void resetAudio() {
        audioBuffer.clear();
        audioBuffer.resize(maxBuffer);
    }

    vector<float> forward(vector<float>& audioChunk) {
        vector <float> outSpeakerChangeTimes;
        int startOffsetNum = 0;
        float startOffsetTime;

        int chunkSamples = audioChunk.size();

        float maxLookBackTime = 1.0f;   // for how many seconds before current audio chunk onset can we find timestamps
        int maxLookBackNum = (int)(maxLookBackTime * (float)sampleRate);

        audioBuffer.insert(audioBuffer.end(), audioChunk.begin(), audioChunk.end());
        if (chunkSamples < maxBuffer) {
            startOffsetNum = maxBuffer - chunkSamples;
            audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + chunkSamples);
        }
        else {
            startOffsetNum = chunkSamples - maxBuffer;
            audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + audioBuffer.size() - maxBuffer);
        }
        startOffsetTime = (float)startOffsetNum / (float)sampleRate;

        //wprintf(L"audioBuffer.size() = %i, startOffsetTime = %.2f\n", audioBuffer.size(), startOffsetTime);

        // TODO: make sure that min-max normalizing audio is OK
        vector<float> audioBuffEmph = PreemphasizeAndNormalize(audioBuffer);

        // Important:
        //   don't normalize log mel *features*
        //   feat is shape [time, numFilters] flattened
        vector<float> mel = melCompute.computeLogMel(audioBuffEmph, false, false);
        int n_mel = mel.size() / numMelFilters;
        std::vector<int64_t> inputChunkDims = { 1, (int64_t)n_mel, numMelFilters };

        std::vector<int64_t> inputChunkLengthDims = { 1 };
        vector<int64_t> inputChunkLength = { (int64_t)n_mel };

        float dummyVal = 0;
        std::vector<int64_t> inputSpkCacheDims = { 1, 0, 512 };
        std::vector<int64_t> inputSpkCacheLengthDims = { 1 };
        vector<int64_t> inputSpkCacheLength = { 0 };
        std::vector<int64_t> inputFifoDims = { 1, 0, 512 };
        std::vector<int64_t> inputFifoLengthDims = { 1 };
        vector<int64_t> inputFifoLength = { 0 };

        const char* inputNames[] = {
            "chunk",
            "chunk_lengths",
            "spkcache",
            "spkcache_lengths",
            "fifo",
            "fifo_lengths",
        };

        const char* outputNames[] = {
            "spkcache_fifo_chunk_preds",
            "chunk_pre_encode_embs",
            "chunk_pre_encode_lengths",
        };

        std::vector<Ort::Value> inputTensors;
        // chunk
        inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, mel.data(), mel.size(), inputChunkDims.data(), inputChunkDims.size()));
        // chunk_lengths
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputChunkLength.data(), inputChunkLength.size(), inputChunkLengthDims.data(), inputChunkLengthDims.size()));
        // spkcache
        inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, &dummyVal, 0, inputSpkCacheDims.data(), inputSpkCacheDims.size()));
        // spkcache_lengths
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputSpkCacheLength.data(), inputSpkCacheLength.size(), inputSpkCacheLengthDims.data(), inputSpkCacheLengthDims.size()));
        // fifo
        inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, &dummyVal, 0, inputFifoDims.data(), inputFifoDims.size()));
        // spkcache_lengths
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputFifoLength.data(), inputFifoLength.size(), inputFifoLengthDims.data(), inputFifoLengthDims.size()));

        auto t0 = chrono::high_resolution_clock::now();
        auto outputTensors = model->Run(Ort::RunOptions{ nullptr }, inputNames, inputTensors.data(), inputTensors.size(), outputNames, 3);
        auto t1 = chrono::high_resolution_clock::now();

#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "sortformer timing: %lli [ms]", chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());
#endif
        //wcout << "sortformer timing: " << (double)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count() / 1000.0 << endl;

        float* logits = outputTensors.front().GetTensorMutableData<float>();
        Ort::TensorTypeAndShapeInfo outTypeShapeInfo = outputTensors.front().GetTensorTypeAndShapeInfo();
        vector<int64_t> outShape = outTypeShapeInfo.GetShape();
        //wcout << outShape[0] << ", " << outShape[1] << ", " << outShape[2] << endl;

        // Convert logits (shape [1, numTime, numSpk]) to probabilities
        int numTime = outShape[1];  // in 80ms increments
        int numSpk = outShape[2];   // i.e. 4
        vector<float> probs(logits, logits + (numTime * numSpk));
        for (int i = 0; i < numTime; i++) {
            float den = 0.0f;
            for (int j = 0; j < numSpk; j++) {
                float logit = logits[i * numSpk + j];
                den += exp(logit);
            }

            for (int j = 0; j < numSpk; j++)
                probs[i * numSpk + j] = exp(logits[i * numSpk + j]) / den;
        }

        // Determine minimal time that will be used in speaker change detection time points
        // This is done, so that when streaming chunks are being input, subsequent forward passes won't return overlapping output times
        // --> i.e.: only newly detected speaker change timepoints are being output
        int minNum;
        float minTime;
        if (chunkSamples > maxBuffer)   // it was completely overriden + truncated to maxBuffer
            minNum = 0;
        else
            minNum = std::max(0, maxBuffer - chunkSamples - maxLookBackNum);

        minTime = (float)minNum / (float)sampleRate;
        //wprintf(L"minTime = %.2f\n", minTime);

        // Find speaker change time points (include first speaker onset as well)
        int prevSpeaker = -1;

        for (int i = 0; i < numTime; i++) {
            int currSpeaker = -1;
            float currSpeakerProb = 0.0f;

            for (int j = 0; j < numSpk; j++) {
                float prob = probs[i * numSpk + j];
                if (prob > minProb && prob > currSpeakerProb) {
                    currSpeaker = j;
                    currSpeakerProb = prob;
                }

                if (currSpeaker != -1 && currSpeaker != prevSpeaker) {
                    float time = (float)i * timeUnitSortformer;
                    if (time >= minTime) {
                        float timeWithoutOffset = time - startOffsetTime;   // time of spk.chg. relative to new chunk start
                        outSpeakerChangeTimes.push_back(timeWithoutOffset);
                        //wprintf(L"SPK%i @%.2f sec\n", j, timeWithoutOffset);
                    }
                    prevSpeaker = currSpeaker;
                }
            }
        }

        return outSpeakerChangeTimes;
    }

    // Implementation which removes 'duplicates', e.g. when 2 subsequent forward passes return similar timestamps
    // Return timestamp values are still relative to beginning of currently input audio chunk
    vector<float> forward2(vector<float>& audioChunk) {
        float minTimeDiff = 0.80f;
        static float lastMaxTime = -12.34f;
        static float time = 0.0f;

        vector<float> timestamps = forward(audioChunk);
        vector<float> modTimestamps;

        for (int i = 0; i < timestamps.size(); i++) {
            float actualTime = timestamps[i] + time;
            if (actualTime - lastMaxTime > minTimeDiff)
                modTimestamps.push_back(timestamps[i]);
        }

        if (modTimestamps.size())
            lastMaxTime = modTimestamps[modTimestamps.size() - 1] + time;

        time += (float)audioChunk.size() / (float)sampleRate;

        return modTimestamps;
    }
};
