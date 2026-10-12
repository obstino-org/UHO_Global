// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <memory>
#include "onnxruntime/onnxruntime_cxx_api.h"
#include "logmel_nemo.h"
using namespace std;

/*
static vector<float> PreemphasizeAndNormalize(vector<float>& in)
{
    double filter_factor = 0.97;
    vector<float> dout;

    int num = in.size();
    float dmax;

    dout = vector<float>(num);

    dmax = 0.0f;
    for (int k = 1; k < (int)num; k++) {
        dout[k] = (float)in[k] - filter_factor * (double)in[k - 1];
        if (fabs(dout[k]) > dmax)
            dmax = fabs(dout[k]);
    }

    for (int k = 0; k < (int)num; k++)
        dout[k] = (float)(dout[k] / dmax);

    return dout;
}

bool ReadWaveIntoBuffer(const char* szFile, vector<short>& sout, uint32_t* numEl, uint32_t* sampleRate, uint32_t force_num_samples)	// assumes we have 1 channel, 16 bit
{
    FILE* f;
    uint32_t Subchunk1Size, Subchunk2Size;
    char buff[100] = { 0 };
    fopen_s(&f, szFile, "rb");
    if (!f)
        return false;

    fseek(f, 16, 0);
    fread(&Subchunk1Size, sizeof(uint32_t), 1, f);
    fseek(f, 24, 0);
    fread(sampleRate, sizeof(uint32_t), 1, f);
    fseek(f, 16 + 4 + Subchunk1Size + 4, 0);
    fread(&Subchunk2Size, sizeof(uint32_t), 1, f);
    if (force_num_samples > 0) {
        sout = vector<short>(force_num_samples, 0);
        uint32_t read_size = std::min(force_num_samples * 2, Subchunk2Size);
        fread(&sout[0], 1, read_size, f);
        *numEl = force_num_samples;
    }
    else {
        sout = vector<short>(Subchunk2Size / 2, 0);
        fread(&sout[0], 1, Subchunk2Size, f);
        *numEl = Subchunk2Size / 2;
    }

    fclose(f);

    return true;
}

vector<float> readWave(string path)
{
    vector<short> pcmShort;
    vector<float> pcmFloat;
    uint32_t numEl;
    uint32_t sampleRate;
    ReadWaveIntoBuffer(path.c_str(), pcmShort, &numEl, &sampleRate, 0);
    for (int i = 0; i < numEl; i++)
        pcmFloat.push_back((float)pcmShort[i] / 32768.0);
    return pcmFloat;
}*/

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

	NeMoSortformer(AAssetManager *manager, string modelPath = "sf21.int8.shrink.onnx", float memoryWindow = 5.0f) :
		memoryWindow(memoryWindow), melCompute(manager, "melfilters128.bin", numMelFilters)
	{
		try {
			options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
			env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, "nemo_sortformer_inference");
			//model = make_unique<Ort::Session>(env, modelPath.c_str(), options);
            vector<uint8_t> modelData = readAsset(manager, (char*)modelPath.c_str());
            model = make_unique<Ort::Session>(Ort::Session(env, modelData.data(), modelData.size(), options));
        }
		catch (const Ort::Exception& e) {
			std::cerr << "Error creating session: " << e.what() << std::endl;
		}

        maxBuffer = (int)((float)sampleRate * memoryWindow);

        resetAudio();
	}

    vector<uint8_t> readAsset(AAssetManager *manager, char *assetFileName) {
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "Opening asset with name %s", assetFileName);
        AAsset *asset = AAssetManager_open(manager, assetFileName, AASSET_MODE_BUFFER);
        if(asset == nullptr) {
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "asset is NULL");
            return {};
        }
        else
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "asset is OK");

        uint8_t *buffer = (uint8_t*)AAsset_getBuffer(asset);
        long len = AAsset_getLength(asset);
        vector<uint8_t> v(buffer, buffer + len);
        AAsset_close(asset);
        return v;
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
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "sortformer timing: %lli [ms]", chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());
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
    vector<float> forward2(vector<float> &audioChunk) {
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
