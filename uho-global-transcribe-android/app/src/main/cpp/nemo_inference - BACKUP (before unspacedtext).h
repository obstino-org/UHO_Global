// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "onnxruntime/onnxruntime_cxx_api.h"
#include "logmel_nemo.h"
#include "stream.h"
#include "dtw.h"
using namespace std;

static vector<float> PreemphasizeAndNormalize(vector<float>& in)
{
    float filter_factor = 0.97f;
    vector<float> dout;

    int num = (int)in.size();
    float dmax;

    dout = vector<float>(num);

    dmax = 0.0f;
    for (int k = 1; k < (int)num; k++) {
        dout[k] = (float)in[k] - filter_factor * in[k - 1];
        if (fabs(dout[k]) > dmax)
            dmax = fabs(dout[k]);
    }

    for (int k = 0; k < (int)num; k++)
        dout[k] = (float)(dout[k] / dmax);

    return dout;
}

/*bool ReadWaveIntoBuffer(const char* szFile, vector<short>& sout, uint32_t* numEl, uint32_t* sampleRate, uint32_t force_num_samples)	// assumes we have 1 channel, 16 bit
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

class NeMoInference {
public:
    LogMelNeMo logMelInst;
    vector<wstring> toks;
    Ort::Env env;
    unique_ptr<Ort::Session> model;
    Ort::SessionOptions options;
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    std::locale myLocale = std::locale("C.UTF-8");

    int blkTokenId;
    wstring spaceTokenText = L"\u2581";
    int spaceTokenId;
    vector<int> tokensToSuppress;

    class NeMoOut {
    public:
        vector<int> rawOutTokens;
        vector<float> rawOutProbs;

        vector<int> uniqueOutTokens;
        vector<float> uniqueOutProbs;
        vector<float> uniqueTokTimestamps;

        float firstSilenceTime = 0.0f;
        float lastSilenceTime = 0.0f;

        NeMoOut(vector<int> rawOutTokens, vector<float> rawOutProbs, vector<int> uniqueOutTokens, vector<float> uniqueOutProbs, vector<float> uniqueTokTimestamps, float firstSilenceTime, float lastSilenceTime) :
                rawOutTokens(rawOutTokens),
                rawOutProbs(rawOutProbs),
                uniqueOutTokens(uniqueOutTokens),
                uniqueOutProbs(uniqueOutProbs),
                uniqueTokTimestamps(uniqueTokTimestamps),
                firstSilenceTime(firstSilenceTime),
                lastSilenceTime(lastSilenceTime)
        {}

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
            std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
            return myconv.to_bytes(str);
        }

        std::u32string wstring_to_utf32(std::wstring& str)
        {
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
            return converter.from_bytes(wstring_to_utf8(str));
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

            for (int i = 0; i < num; i++) {
                if (std::find(spkChgIndexes.begin(), spkChgIndexes.end(), i) != spkChgIndexes.end()) {
                    if (charBased == true) // for char based, prepend space before spkChgSymbol
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

    void prepareTokenData(AAssetManager *manager, string tokensPath, bool installTime) {
        toks = readTokens(manager, tokensPath, installTime);
        blkTokenId = toks.size() - 1;
        for (int i = 0; i < toks.size(); i++) {
            if (toks[i] == spaceTokenText) { // spaceTokenText
                spaceTokenId = i;
                break;
            }
        }

        std::locale::global(std::locale(""));   // important for std::iswalpha
        for (int i = 0; i < toks.size(); i++) {
            if (i != spaceTokenId && !isWstringAlpha(toks[i])) {
                tokensToSuppress.push_back(i);
                //wcout << toks[i] << endl;
            }
        }
    }

    NeMoInference(AAssetManager *manager, string modelPath="model.onnx", string vocabPath="tokens.txt", bool installTime=true): logMelInst(manager) {
        __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "reading tokens");
        prepareTokenData(manager, vocabPath, installTime);

        env = Ort::Env(
                OrtLoggingLevel::ORT_LOGGING_LEVEL_VERBOSE,//OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                "nemo_inference");
        options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
        //options.SetGraphOptimizationLevel(ORT_DISABLE_ALL);
        //options.DisableCpuMemArena();

        //uint32_t nnapi_flags = 0;
        //nnapi_flags |= NNAPI_FLAG_USE_FP16;
        //Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_Nnapi(options, nnapi_flags));

        // DO NOT USE NNAPI, on Samsung Tab S7 it leads to SLOW execution

        vector<uint8_t> modelData;
        if(installTime)
            modelData = readAsset(manager, (char*)modelPath.c_str()); //"model.onnx");
        else
            modelData = readFile(modelPath);

        try {
            model = make_unique<Ort::Session>(Ort::Session(env, modelData.data(), modelData.size(), options));
        }
        catch (Ort::Exception& e) {
            __android_log_print(ANDROID_LOG_INFO, "UHO_Transcribe", "%s", e.what());
        }
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

    vector<uint8_t> readFile(string filePath) {
        vector<uint8_t> ret;

        ifstream f(filePath, std::ios::binary | std::ios::ate);
        if(!f.is_open())
            return ret;

        streamsize size = f.tellg();
        ret = vector<uint8_t>(size);

        f.seekg(std::ios::beg);
        f.read((char*)ret.data(), size);
        f.close();

        return ret;
    }

    vector<wstring> readTokens(AAssetManager *manager, string tokensFile="tokens.txt", bool installTime=true) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        vector<wstring> toks;

        vector<uint8_t> vocabVect;
        if(installTime)
            vocabVect = readAsset(manager, (char*)tokensFile.c_str());
        else
            vocabVect = readFile(tokensFile);

        string strVocab(vocabVect.begin(), vocabVect.end());
        wstring wstrVocab = converter.from_bytes(strVocab);

        wstringstream f;
        f.imbue(myLocale);
        f = wstringstream(wstrVocab);

        while (!f.eof()) {
            wstring s;
            int n;
            f >> s >> n;
            toks.push_back(s);
            //wcout << s << ", " << n << endl;
        }

        /*
        wstring strVocab;
        for (uint8_t c : vocabVect) {
            wchar_t w = (wchar_t)c;
            strVocab += w;
        }

        wstringstream f(strVocab);
        while (!f.eof()) {
            wstring s;
            int n;
            f >> s >> n;
            toks.push_back(s);
            // wcout << s << ", " << n << endl;
        }*/

        return toks;
    }

    NeMoOut forward(vector<float>& x) {
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
        // wcout << (double)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count() / 1000.0 << endl;

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
                uniqueTokTimestamps.push_back((float)i / (float)numTokens * (float)x.size() / (float)16000.0f);

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

        /*
        // Apply compress factor
        for(int i = 0; i < uniqueTokTimestamps.size(); i++)
            uniqueTokTimestamps[i] *= compressFactor;
        firstSilenceTime *= compressFactor;
        lastSilenceTime *= compressFactor;*/

        NeMoOut output = NeMoOut(rawOutTokens, rawOutProbs, uniqueOutTokens, uniqueOutProbs, uniqueTokTimestamps, firstSilenceTime, lastSilenceTime);
        return output;
    }

    WordLevelOutput convertTokensToWords(NeMoInference::NeMoOut& tokenInfo)
    {
        WordLevelOutput out;
        wchar_t spaceChar = this->toks[spaceTokenId][0];

        int numTokens = tokenInfo.uniqueOutTokens.size();

        bool saveTimestampInNextIteration = false;
        wstring word = L"";

        int i = 0;
        while (i < numTokens) {
            int token = tokenInfo.uniqueOutTokens[i];
            float timestamp = tokenInfo.uniqueTokTimestamps[i];
            wstring tokStr = this->toks[token];

            // don't save non-alphanumeric tokens
            if (std::find(tokensToSuppress.begin(), tokensToSuppress.end(), token) != tokensToSuppress.end()) {
                if ((i + 1) == numTokens && word.length())
                    out.words.push_back(word);
                i++;
                continue;
            }

            if (saveTimestampInNextIteration) {
                saveTimestampInNextIteration = false;
                if (tokStr[0] != spaceChar)
                    out.timestamps.push_back(timestamp);
            }

            if (tokStr[0] == spaceChar) {
                if (word.length()) {
                    out.words.push_back(word);
                    word = L"";
                }

                // change "_hi" to "hi" (or "_" to "")
                tokStr = tokStr.substr(1);

                if (tokStr.length() > 0) { // if it was e.g. "_hi" this condition will be true
                    out.timestamps.push_back(timestamp);
                }
                else {  // if it was only "_" this condition will be true ==> save timestamp in next iteration
                    saveTimestampInNextIteration = true;
                }
            }
            else if (i == 0) {
                out.timestamps.push_back(timestamp);
            }

            word += tokStr;

            if ((i + 1) == numTokens && word.length())
                out.words.push_back(word);

            i++;
        }

        /*
        // Code for debugging
        if (out.timestamps.size() != out.words.size())
        {
            // wprintf(L"!!! %i timestamps, %i words !!! (printing...)", out.timestamps.size(), out.words.size());
            // wcout << out.toString() << endl << out.toStringTimestamps() << endl;
            for (int i = 0; i < tokenInfo.uniqueOutTokens.size(); i++)
                wprintf(L"%s (tok=%i, time=%.2f)\n", this->toks[tokenInfo.uniqueOutTokens[i]].c_str(), tokenInfo.uniqueOutTokens[i], tokenInfo.uniqueTokTimestamps[i]);

            //for (int i = 0; i < out.words.size(); i++) {
            //    wprintf(L"%s (%i)\n", out.words[i].c_str());
            //}
            getchar();
        }
        */

        return out;
    }
};
