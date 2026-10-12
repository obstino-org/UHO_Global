// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include "logmel_nemo.h"
#include "common_io.h"

#if defined(__ANDROID__)
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "onnxruntime/onnxruntime_cxx_api.h"
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <cwctype>
#include "onnxruntime_cxx_api.h"
#endif

using namespace std;

class NeMoInference {
public:
#if defined(__ANDROID__)
	std::locale myLocale = std::locale("C.UTF-8");
#elif defined(_WIN32)
    std::locale myLocale = std::locale("en_US.UTF-8");
#endif
    LogMelNeMo logMelInst;
    vector<wstring> toks;
    Ort::Env env;
    unique_ptr<Ort::Session> model;
    Ort::SessionOptions options;
	Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
	
    wstring modelPath;
    wstring vocabPath;

    int blkTokenId;
    wstring blkTokenText = L"<blk>";
    wstring spaceTokenText = L"\u2581";
    int spaceTokenId;
    vector<int> tokensToSuppress;
    bool unspacedText;

    class NeMoOut {
    public:
        vector<int> rawOutTokens;
        vector<float> rawOutProbs;

        vector<int> uniqueOutTokens;
        vector<float> uniqueOutProbs;
        vector<float> uniqueTokTimestamps;
        vector<int> uniqueTokIndexes;

        float firstSilenceTime = 0.0f;
        float lastSilenceTime = 0.0f;

        NeMoOut(vector<int> rawOutTokens, vector<float> rawOutProbs, vector<int> uniqueOutTokens, vector<float> uniqueOutProbs, vector<float> uniqueTokTimestamps, vector<int> uniqueTokIndexes, float firstSilenceTime, float lastSilenceTime) :
            rawOutTokens(rawOutTokens),
            rawOutProbs(rawOutProbs),
            uniqueOutTokens(uniqueOutTokens),
            uniqueOutProbs(uniqueOutProbs),
            uniqueTokTimestamps(uniqueTokTimestamps),
            uniqueTokIndexes(uniqueTokIndexes),
            firstSilenceTime(firstSilenceTime),
            lastSilenceTime(lastSilenceTime)
        {}

        NeMoOut() { }

    };

    class WordLevelOutput {
    public:
        vector<wstring> words;
        vector<float> timestamps;
        vector<float> probs;
        vector<int> rawTokWordIndexes;
        vector<int> spkChgIndexes;
        wstring spkChgSymbol = L">>";

        bool unspacedText;

        WordLevelOutput(bool unspacedText = false) : unspacedText(unspacedText) {}

        // Deep copy
        WordLevelOutput(const WordLevelOutput& wlo) {
            words = wlo.words;
            timestamps = wlo.timestamps;
            rawTokWordIndexes = wlo.rawTokWordIndexes;
            probs = wlo.probs;
            unspacedText = wlo.unspacedText;
            spkChgIndexes = wlo.spkChgIndexes;
        }

        std::string wstring_to_utf8(std::wstring& str) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv("");
            return myconv.to_bytes(str);
        }

        std::u32string wstring_to_utf32(std::wstring& str)
        {
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
            std::u32string out;
            try {
                out = converter.from_bytes(wstring_to_utf8(str));
            } catch(exception& e) { }
            return out;
        }

        bool isChineseCharacter(char32_t c) {
            uint32_t code = (uint32_t)c;
            return ((code >= 0x4E00 && code <= 0x62FF) ||
                (code >= 0x6300 && code <= 0x77FF) ||
                (code >= 0x7800 && code <= 0x8CFF) ||
                (code >= 0x8D00 && code <= 0x9FCC) ||
                (code >= 0x3400 && code <= 0x4DB5) ||
                (code >= 0x20000 && code <= 0x215FF) ||
                (code >= 0x21600 && code <= 0x230FF) ||
                (code >= 0x23100 && code <= 0x245FF) ||
                (code >= 0x24600 && code <= 0x260FF) ||
                (code >= 0x26100 && code <= 0x275FF) ||
                (code >= 0x27600 && code <= 0x290FF) ||
                (code >= 0x29100 && code <= 0x2A6DF) ||
                (code >= 0x2A700 && code <= 0x2B734) ||
                (code >= 0x2B740 && code <= 0x2B81D) ||
                (code >= 0x2B820 && code <= 0x2CEAF) ||
                (code >= 0x2CEB0 && code <= 0x2EBEF) ||
                (code >= 0x2F800 && code <= 0x2FA1F));
        }

        bool containsChineseCharacter(wstring str) {
            u32string strUTF32 = wstring_to_utf32(str);
            for (char32_t c : strUTF32)
                if (isChineseCharacter(c))
                    return true;
            return false;
        }

        wstring toString(bool removeLast = false) {
            wstring res = L"";
            bool charBased = unspacedText;
            int num = std::max((int)words.size() - (removeLast ? 1 : 0), 0);
            bool prevSkipCondition = false;

            for (int i = 0; i < num; i++) {
                if (std::find(spkChgIndexes.begin(), spkChgIndexes.end(), i) != spkChgIndexes.end()) {
                    if (charBased == true || prevSkipCondition == true) // for char based and Chinese, prepend space before spkChgSymbol
                        res += L" ";
                    res += spkChgSymbol + L" ";
                }
                res += words[i];

                // if current and next word is a Chinese character, don't add space
                bool skipCondition = false;
                if ((i + 1) < num &&
                    containsChineseCharacter(words[i]) &&
                    containsChineseCharacter(words[i + 1]))
                    skipCondition = true;

                prevSkipCondition = skipCondition;

                if (unspacedText || skipCondition)
                    continue;

                // if next word is punctuation, don't add space after current word
                if ((i + 1) < num && !words[i + 1].empty() && ispunct(words[i + 1][0]))
                    continue;

                if(i != (num-1))
                    res += L" ";
            }
            return res;
        }

        // unlike toString, this function adds * character whenever there's been enduring silence in speech
        // (toString2 currently not it use, and not up to date)
        wstring toString2(float enduringSilenceTime = 2.0f, bool removeLast = false) {
            // TODO: spk chg symbol support
            wstring res = L"";

            int num = std::max((int)words.size() - (removeLast ? 1 : 0), 0);

            for (int i = 0; i < num; i++) {
                if (i > 0 && (timestamps[i] - timestamps[i - 1]) > enduringSilenceTime)
                    res += L"*";

                res += words[i];

                // if current and next word is a Chinese character, don't add space
                bool skipCondition = false;
                if ((i + 1) < num &&
                    containsChineseCharacter(words[i]) &&
                    containsChineseCharacter(words[i + 1]))
                    skipCondition = true;
                
                if (unspacedText || skipCondition)
                    continue;

                // if next word is punctuation, don't add space after current word
                if ((i + 1) < num && !words[i + 1].empty() && ispunct(words[i + 1][0]))
                    continue;

                res += L" ";
            }

            return res;
        }

        wstring toStringTimestamps() {
            wstring res = L"";
            for (int i = 0; i < timestamps.size(); i++)
                res += std::to_wstring(timestamps[i]) + L" ";
            return res;
        }
    };

    bool isWstringAlpha(wstring s) {
        // (!) before calling this function, make sure to set std::locale::global(std::locale(""));
        for (int i = 0; i < s.size(); i++) {
            if (!std::iswalpha(s[i]) && s[i] != spaceTokenText[0])
                return false;
        }
        return true;
    }

#if defined(__ANDROID__)
    void prepareTokenData(AAssetManager *manager, string tokensPath, bool suppressTokens, bool installTime) {
#elif defined(_WIN32)
    void prepareTokenData(bool suppressTokens = false) {
#endif

#if defined(__ANDROID__)
		readTokens(manager, tokensPath, installTime);
#elif defined(_WIN32)
        readTokens();
#endif
        blkTokenId = toks.size() - 1;
        for (int i = 0; i < toks.size(); i++) {
            if (toks[i] == blkTokenText) {
                blkTokenId = i;
                break;
            }
        }

        for (int i = 0; i < toks.size(); i++) {
            if (toks[i] == spaceTokenText) { // spaceTokenText
                spaceTokenId = i;
                break;
            }
        }

        // this is useful for Slovene model (clarin.si), as some tokens are used for filler words
        // -- those tokens are usually not alphabetical in the vocab
        if (suppressTokens) {	// todo check if this is functional in all languages (most models don't set this flag)
            std::locale::global(std::locale(""));   // important for std::iswalpha
            for (int i = 0; i < toks.size(); i++) {
                if (i != spaceTokenId && !isWstringAlpha(toks[i])) {
                    tokensToSuppress.push_back(i);
                }
            }
        }
    }

#if defined(__ANDROID__)
    NeMoInference(AAssetManager *manager, string modelPath="model.onnx", string vocabPath="tokens.txt", bool unspacedText = false, bool suppressTokens = false, bool installTime = false): logMelInst(manager) {
#elif defined(_WIN32)
    NeMoInference(wstring modelPath, wstring vocabPath, bool useCUDA = false, bool unspacedText = false, bool suppressTokens = false) : modelPath(modelPath), vocabPath(vocabPath) {
	#endif
        this->unspacedText = unspacedText;

#if defined(__ANDROID__)
        prepareTokenData(manager, vocabPath, suppressTokens, installTime);
#elif defined(_WIN32)
        prepareTokenData(suppressTokens);
#endif

        env = Ort::Env(
            OrtLoggingLevel::ORT_LOGGING_LEVEL_VERBOSE,//OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
            "nemo_inference");
        options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

#if defined(_WIN32)
        if (useCUDA) {
            OrtCUDAProviderOptions cudaOptions;
            cudaOptions.device_id = 0;
            options.AppendExecutionProvider_CUDA(cudaOptions);
        }
#endif

        vector<uint8_t> modelData;
#if defined(__ANDROID__)
        if(installTime)
            modelData = readAsset(manager, (char*)modelPath.c_str());
        else
            modelData = readFile(modelPath);
#elif defined(_WIN32)
		modelData = readFile(string(modelPath.begin(), modelPath.end()));
#endif

        try {
            model = make_unique<Ort::Session>(Ort::Session(env, modelData.data(), modelData.size(), options));
        }
        catch (Ort::Exception& e) {
#if defined(__ANDROID__)
            __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "%s", e.what());
#endif
        }
    }

#if defined(__ANDROID__)
	vector<wstring> readTokens(AAssetManager *manager, string tokensFile="tokens.txt", bool installTime=true) {
#elif defined(_WIN32)
    vector<wstring> readTokens() {
#endif
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        vector<uint8_t> vocabVect;
#if defined(__ANDROID__)
        if(installTime)
            vocabVect = readAsset(manager, (char*)tokensFile.c_str());
        else
            vocabVect = readFile(tokensFile);
#elif defined(_WIN32)
        vocabVect = readFile(string(vocabPath.begin(), vocabPath.end()));
#endif

        string strVocab(vocabVect.begin(), vocabVect.end());
        wstring wstrVocab;
        try {
            wstrVocab = converter.from_bytes(strVocab);
        }
        catch (exception& e) {
            wstrVocab = wstring(strVocab.begin(), strVocab.end());
        }

        wstringstream f;
        f = wstringstream(wstrVocab);
        f.imbue(myLocale);

        while (!f.eof()) {
            wstring s;
            int n;
            f >> s >> n;
            toks.push_back(s);
            //wcout << s << ", " << n << endl;
        }

        return toks;
    }

    NeMoOut forward(vector<float>& x) {
        if (!x.size())
            return NeMoOut();
		
        vector<float> mel = logMelInst.computeLogMel(x);
        int n_mel = mel.size() / 80;

        std::vector<int64_t> inputAudioDims = { 1, 80, (int64_t)n_mel };
        std::vector<int64_t> inputLengthDims = { 1 };
        vector<int64_t> inputLength = { (int64_t)n_mel };
        const char* inputNames[] = { "audio_signal", "length" };

        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, mel.data(), mel.size(), inputAudioDims.data(), inputAudioDims.size()));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputLength.data(), inputLength.size(), inputLengthDims.data(), inputLengthDims.size()));

        const char* outputNames[] = { "logprobs" };

        auto t0 = chrono::high_resolution_clock::now();
        auto outputTensors = model->Run(Ort::RunOptions{ nullptr }, inputNames, inputTensors.data(), inputTensors.size(), outputNames, 1);
        auto t1 = chrono::high_resolution_clock::now();
        //wcout << (double)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count() / 1000.0 << endl;

        float* logProbs = outputTensors.front().GetTensorMutableData<float>();
        Ort::TensorTypeAndShapeInfo outTypeShapeInfo = outputTensors.front().GetTensorTypeAndShapeInfo();
        vector<int64_t> outShape = outTypeShapeInfo.GetShape();

        // Example output is "Out shape : 1; 750; 129;" (129 is number of possible tokens, 750 is number of output tokens)
        //wcout << "Out shape: ";
        //for (int i = 0; i < outShape.size(); i++)
        //    wcout << outShape[i] << "; ";
        //wcout << endl;

        int numTokens = outShape[1];
        int numVocab = outShape[2];

        vector<int> rawOutTokens;
        vector<float> rawOutProbs;

        vector<int> uniqueOutTokens;
        vector<float> uniqueOutProbs;
        vector<float> uniqueTokTimestamps;
        vector<int> uniqueTokIndexes;

        /*
        // test code only
        vector<int> testIds =     { 1,   1,   2,   2,   5,   6,   8,   8 };
        vector<float> testProbs = { 0.5f,0.9f,0.3f,0.9f,0.9f,0.8f,0.7f,0.8f };
        numTokens = testIds.size();
        */

        for (int i = 0; i < numTokens; i++) {
            // argmax over token probability distribution

            vector<float> iTokenProbs;
            for (int j = 0; j < numVocab; j++)
                iTokenProbs.push_back(logProbs[i * numVocab + j]);
            auto it = std::max_element(iTokenProbs.begin(), iTokenProbs.end());
            int id = it - iTokenProbs.begin();
            float prob = exp(*it);  // convert log probs to probs

            rawOutTokens.push_back(id);
            rawOutProbs.push_back(prob);
        }

        // collapse duplicate tokens and find max probability of that token (in case its repeated)
        // additionally store timestamps for each id
        int prevId = blkTokenId;
        float prevMaxProb = -1.0f;
        for (int i = 0; i < numTokens; i++) {
            //int id = testIds[i];
            //float prob = testProbs[i];

            int id = rawOutTokens[i];
            float prob = rawOutProbs[i];
            if (prevId == id) {
                prevMaxProb = std::max(prob, prevMaxProb);
                continue;
            }
            prevId = id;

            if (id != blkTokenId) {
                uniqueOutTokens.push_back(id);
                uniqueTokTimestamps.push_back((float)i / (float)(numTokens - 1) * (float)x.size() / (float)16000.0f);
                uniqueTokIndexes.push_back(i);

                if (prevMaxProb != -1.0f)
                    uniqueOutProbs.push_back(prevMaxProb);
            }
            prevMaxProb = prob;
        }

        if (numTokens > 0)
            uniqueOutProbs.push_back(prevMaxProb);

        // Now uniqueOutTokens and uniqueOutProbs contain values we can use

        // Count silence at end
        int numLastBlank = 0;
        for (int i = numTokens - 1; i >= 0; i--) {
            int t = rawOutTokens[i];
            if (t == blkTokenId)
                numLastBlank++;
            else
                break;
        }
        float lastSilenceTime = (float)numLastBlank / (float)rawOutTokens.size() * (float)x.size() / (float)16000.0f;

        // Count silence at start
        int numFirstBlank = 0;
        for (int i = 0; i < numTokens; i++) {
            int t = rawOutTokens[i];
            if (t == blkTokenId)
                numFirstBlank++;
            else
                break;
        }
        float firstSilenceTime = (float)numFirstBlank / (float)rawOutTokens.size() * (float)x.size() / (float)16000.0f;

        for (int i = 0; i < uniqueOutTokens.size(); i++) {
            //wprintf(L"%s\t (p=%.2f)\n", toks[uniqueOutTokens[i]], uniqueOutProbs[i]);
            //wprintf(L"%s", toks[uniqueOutTokens[i]]);
        }
        //wprintf(L"\n");

        NeMoOut output = NeMoOut(rawOutTokens, rawOutProbs, uniqueOutTokens, uniqueOutProbs, uniqueTokTimestamps, uniqueTokIndexes, firstSilenceTime, lastSilenceTime);
        return output;
    }

    WordLevelOutput convertTokensUnspaced(NeMoInference::NeMoOut& tokenInfo)
    {
        WordLevelOutput out = WordLevelOutput(unspacedText);

        for (int i = 0; i < tokenInfo.uniqueOutTokens.size(); i++) {
            if(tokenInfo.uniqueOutTokens[i] == spaceTokenId)
                out.words.push_back(L" ");
            else
                out.words.push_back(toks[tokenInfo.uniqueOutTokens[i]]);
            out.timestamps.push_back(tokenInfo.uniqueTokTimestamps[i]);
            out.rawTokWordIndexes.push_back(tokenInfo.uniqueTokIndexes[i]);
            out.probs.push_back(tokenInfo.uniqueOutProbs[i]);
        }

        return out;
    }

    WordLevelOutput convertTokensToWords(NeMoInference::NeMoOut& tokenInfo, int minimumIndex = 5)
    {
        if (!tokenInfo.uniqueOutTokens.size())
            return WordLevelOutput(unspacedText);

        if (unspacedText)
            return convertTokensUnspaced(tokenInfo);

        WordLevelOutput out = WordLevelOutput(unspacedText);
        wchar_t spaceChar = this->toks[spaceTokenId][0];

        int numTokens = tokenInfo.uniqueOutTokens.size();

        bool saveTimestampInNextIteration = false;
        wstring word = L"";

        int i = 0;
        while (i < numTokens) {
            int token = tokenInfo.uniqueOutTokens[i];
            float timestamp = tokenInfo.uniqueTokTimestamps[i];
            float prob = tokenInfo.uniqueOutProbs[i];
            int index = tokenInfo.uniqueTokIndexes[i];

            wstring tokStr = this->toks[token];

            // don't save non-alphanumeric tokens
            if (std::find(tokensToSuppress.begin(), tokensToSuppress.end(), token) != tokensToSuppress.end()) {
                if ((i + 1) == numTokens && word.length()) {
                    out.words.push_back(word);
                    out.probs.push_back(prob);
                }
                i++;
                continue;
            }

            if (saveTimestampInNextIteration) {
                saveTimestampInNextIteration = false;
                if (tokStr[0] != spaceChar) {
                    out.timestamps.push_back(timestamp);
                    out.rawTokWordIndexes.push_back(index);
                }
            }

            if (tokStr[0] == spaceChar) {
                if (word.length()) {
                    out.words.push_back(word);
                    out.probs.push_back(prob);
                    word = L"";
                }

                //wprintf(L"substringing text %s\n", tokStr.c_str());
                // change "_hi" to "hi" (or "_" to "")
                tokStr = tokStr.substr(1);

                if (tokStr.length() > 0) { // if it was e.g. "_hi" this condition will be true
                    //wprintf(L"pushing timestamp: %.3f\n", timestamp);
                    out.timestamps.push_back(timestamp);
                    out.rawTokWordIndexes.push_back(index);
                }
                else {  // if it was only "_" this condition will be true ==> save timestamp in next iteration
                    //wprintf(L"going to save timestamp %.3f in next iteration", timestamp);
                    saveTimestampInNextIteration = true;
                }
            }
            else if (i == 0) {
                out.timestamps.push_back(timestamp);
                out.rawTokWordIndexes.push_back(index);
            }

            word += tokStr;

            if ((i + 1) == numTokens && word.length()) {
                out.words.push_back(word);
                out.probs.push_back(prob);
            }

            i++;
        }

        //
        //// Code for debugging
        //if (out.timestamps.size() != out.words.size())
        //{
        //    wprintf(L"!!! %i timestamps, %i words !!! (printing...)", out.timestamps.size(), out.words.size());
        //    wcout << out.toString() << endl << out.toStringTimestamps() << endl;
        //    for (int i = 0; i < tokenInfo.uniqueOutTokens.size(); i++)
        //        wprintf(L"%s (tok=%i, time=%.2f)\n", this->toks[tokenInfo.uniqueOutTokens[i]].c_str(), tokenInfo.uniqueOutTokens[i], tokenInfo.uniqueTokTimestamps[i]);

        //    //for (int i = 0; i < out.words.size(); i++) {
        //    //    wprintf(L"%s (%i)\n", out.words[i].c_str());
        //    //}
        //    getchar();
        //}
        //

        // skip first word if index is too small
        if (out.rawTokWordIndexes.size() && out.rawTokWordIndexes[0] < minimumIndex) {
            //wprintf(L"Correcting first word...\n");
            //out.rawTokWordIndexes.erase(out.rawTokWordIndexes.begin());
            for (int j = minimumIndex; j < tokenInfo.rawOutTokens.size(); j++) {
                if (tokenInfo.rawOutTokens[j] != blkTokenId) {
                    out.rawTokWordIndexes[0] = j;
                    break;
                }
            }

            //out.probs.erase(out.probs.begin());
            //out.timestamps.erase(out.timestamps.begin());
            //out.words.erase(out.words.begin());
        }

        return out;
    }
};
