// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#if defined(__ANDROID__)
#include "onnxruntime/onnxruntime_cxx_api.h"
#elif defined(_WIN32)
#include "onnxruntime_cxx_api.h"
#endif

#include "logmel_nemo.h"
#include "common_io.h"
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

#if defined(__ANDROID__)
	NeMoVAD(AAssetManager *manager, string modelPath, float bufferSizeSeconds=5.0f): computeFeatures(manager) {
#elif defined(_WIN32)
	NeMoVAD(wstring modelPath, float bufferSizeSeconds=5.0f) {
#endif
		try {
			options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
			env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, "nemo_vad_inference");

#if defined(__ANDROID__)			
            vector<uint8_t> modelData = readAsset(manager, (char*)modelPath.c_str());
#elif defined(_WIN32)
			vector<uint8_t> modelData = readFile(string(modelPath.begin(), modelPath.end()));
#endif
            model = make_unique<Ort::Session>(env, modelData.data(), modelData.size(), options);
		}
		catch (const Ort::Exception& e) {
			//std::cerr << "Error creating session: " << e.what() << std::endl;
		}

		bufferSize = (int)((float)sampleRate * bufferSizeSeconds);
		rawAudioBuffer = vector<float>(bufferSize, 0.0f);
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
            // __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "P_speech = %.2f", p);
            bool tmpOut = (p > 0.5f); //(logits[i * 2 + 0] < logits[i * 2 + 1]);
			vadOut.push_back(tmpOut);
		}

		return vadOut;
	}
};