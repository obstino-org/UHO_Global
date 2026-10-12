// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "onnxruntime/onnxruntime_cxx_api.h"
#include "logmel_nemo.h"
#include <chrono>

class NeMoVAD {
public:
	int sampleRate = 16000;
	Ort::Env env;
	Ort::SessionOptions options;
	Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
	unique_ptr<Ort::Session> model;

	LogMelNeMo computeFeatures;

	vector<float> rawAudioBuffer;
	int bufferSize;

	NeMoVAD(AAssetManager *manager, string modelPath, float bufferSizeSeconds=5.0f): computeFeatures(manager) {
		try {
			options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
			env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, "nemo_vad_inference");
            vector<uint8_t> modelData = readAsset(manager, (char*)modelPath.c_str());
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "try4");
            model = make_unique<Ort::Session>(env, modelData.data(), modelData.size(), options);
			//model = make_unique<Ort::Session>(env, modelPath.c_str(), options);
		}
		catch (const Ort::Exception& e) {
			std::cerr << "Error creating session: " << e.what() << std::endl;
		}

		bufferSize = (int)((float)sampleRate * bufferSizeSeconds);
		rawAudioBuffer = vector<float>(bufferSize, 0.0f);
	}

    vector<uint8_t> readAsset(AAssetManager *manager, char *assetFileName) {
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "Opening asset with name %s", assetFileName);
        AAsset *asset = AAssetManager_open(manager, assetFileName, AASSET_MODE_BUFFER);
        if(asset == nullptr)
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "asset is NULL");
        else
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "asset is OK");

        uint8_t *buffer = (uint8_t*)AAsset_getBuffer(asset);
        long len = AAsset_getLength(asset);
        vector<uint8_t> v(buffer, buffer + len);
        AAsset_close(asset);
        return v;
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

	bool streamHasSpeech(vector <float> chunk) {
		vector<bool> vadOut = forward(chunk, true);
		//wcout << "vadOutSize = " << vadOut.size() << endl;

		// vadOut contains VAD outputs for each 20ms segment
		float chunkSeconds = (float)chunk.size() / (float)sampleRate;
		bool hasSpeech = false;
		int numLastSegmentsToCheck = (int)(chunkSeconds / 0.020f);
		// clamp the value
		numLastSegmentsToCheck = std::min((int)vadOut.size(), numLastSegmentsToCheck);

        /*
        // Debug VAD output
        string s = "";
        for (int i = (int)vadOut.size() - numLastSegmentsToCheck; i < (int)vadOut.size(); i++) {
            if (vadOut[i] == true)
                s += "1";
            else
                s += "0";
        }
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "NeMoVAD: %s", s.c_str());
         */

        for (int i = (int)vadOut.size() - 1; i >= (int)vadOut.size() - numLastSegmentsToCheck; i--)
		{
			if (vadOut[i] == true) {
				hasSpeech = true;
				break;
			}
		}

		return hasSpeech;
	}

	vector<bool> forward(vector<float> chunk, bool streaming=true) {
		// return 1.0;	// <-- for testing only
		vector<float> normAudio;

		if (!streaming) {
			normAudio = chunk; //maxNormalize(chunk);
		}
		else {
			// (TODO: test situation where chunk.size() > bufferSize)
			rawAudioBuffer.insert(rawAudioBuffer.end(), chunk.begin(), chunk.end());
			rawAudioBuffer.erase(rawAudioBuffer.begin(), rawAudioBuffer.begin() + chunk.size());
            normAudio = maxNormalize(rawAudioBuffer);
			//normAudio = rawAudioBuffer;
		}

		vector<float> mel = computeFeatures.computeLogMel(normAudio, false, true);
		int n_mel = mel.size() / 80;

		std::vector<int64_t> inputAudioDims = { 1, 80, (int64_t)n_mel };
		std::vector<int64_t> inputLengthDims = { 1 };
		vector<int64_t> inputLength = { (int64_t)n_mel };
		const char* inputNames[] = { "audio_signal" };

		std::vector<Ort::Value> inputTensors;
		inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, mel.data(), mel.size(), inputAudioDims.data(), inputAudioDims.size()));
		
		const char* outputNames[] = { "outputs" };

		//auto t0 = chrono::high_resolution_clock::now();
		auto outputTensors = model->Run(Ort::RunOptions{ nullptr }, inputNames, inputTensors.data(), inputTensors.size(), outputNames, 1);
		//auto t1 = chrono::high_resolution_clock::now();
		//wcout << (double)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count() / 1000.0 << endl;

		float* logits = outputTensors.front().GetTensorMutableData<float>();
		Ort::TensorTypeAndShapeInfo outTypeShapeInfo = outputTensors.front().GetTensorTypeAndShapeInfo();
		vector<int64_t> outShape = outTypeShapeInfo.GetShape();

		// Example output is "Out shape : 1; 1000; 2;" (1000 is number of 20ms segments; 2 are for Voice/Silence class)
		//wcout << "Out shape: ";
		//for (int i = 0; i < outShape.size(); i++)
		//	wcout << outShape[i] << "; ";
		//wcout << endl;

		vector<bool> vadOut;
		int numOutputs = (int)outShape[1];
		for (int i = 0; i < numOutputs; i++) {
			float p = exp(logits[i * 2 + 1]) / (exp(logits[i * 2 + 0]) + exp(logits[i * 2 + 1]));
            // __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "P_speech = %.2f", p);
            bool tmpOut = (p > 0.5f); //(logits[i * 2 + 0] < logits[i * 2 + 1]);
			vadOut.push_back(tmpOut);
		}

		return vadOut;
	}
};