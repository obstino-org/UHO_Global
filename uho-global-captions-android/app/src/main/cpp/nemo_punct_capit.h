// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <map>
#include <regex>
#include <locale>
#include <codecvt>
#include <algorithm>
#include "common_io.h"

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include "onnxruntime/onnxruntime_cxx_api.h"
#elif defined(_WIN32)
#include "onnxruntime_cxx_api.h"
#endif

using namespace std;

enum TokenizerType {
    none = 0,
    RoBERTa = 1,
    BERT = 2
};

class NeMoPC_Tokenizer {
public:
#if defined(__ANDROID__)
    std::locale myLocale = std::locale("C.UTF-8");
#elif defined(_WIN32)
    std::locale myLocale = std::locale("en_US.UTF-8");
#endif

    map<wstring, int> str2tok;
    vector<wstring> tok2str;
    wchar_t spaceToken = L'\u2581'; // This is character L'▁' -- can write directly, but ensure UTF-8 BOM header encoding
    wstring startToken;
    wstring stopToken;
    wstring unkToken;

#if defined(__ANDROID__)
    NeMoPC_Tokenizer(AAssetManager* manager, string path, TokenizerType type = TokenizerType::BERT, bool installTime = true) {
#elif defined(_WIN32)
        NeMoPC_Tokenizer(wstring path, TokenizerType type) {
#endif
        wstring startToken;
        wstring stopToken;

        if (type == TokenizerType::RoBERTa) {
            startToken = L"<s>";
            stopToken = L"</s>";
            unkToken = L"<unk>";
        }
        else if (type == TokenizerType::BERT) {
            startToken = L"[CLS]";
            stopToken = L"[SEP]";
            unkToken = L"[UNK]";
        }

        this->startToken = startToken;
        this->stopToken = stopToken;

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        vector<uint8_t> vocabVect;

        // open vocabulary (path)
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC_Tokenizer reading vocab vect");
        if (installTime)
            vocabVect = readAsset(manager, (char*)path.c_str());
        else
            vocabVect = readFile(path);
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC_Tokenizer read vocab vect");
#elif defined(_WIN32)
        vocabVect = readFile(string(path.begin(), path.end()));
#endif
        string strVocab;
        for (uint8_t c : vocabVect)
            strVocab += (char)c;

        stringstream f;
        //if (!f.is_open())
        //    return;
        f = stringstream(strVocab);
        f.imbue(myLocale);

        while (!f.eof()) {
            string s;
            int n;
            f >> s >> n;

            wstring ws;
            try {
                ws = converter.from_bytes(s);
            }
            catch (exception& e) {
                ws = wstring(s.begin(), s.end());
            }
            if (ws.empty())
                break;

            if (ws[0] == spaceToken)
                ws[0] = L' ';

            tok2str.push_back(ws);
            str2tok[ws] = n;
        }

#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC_Tokenizer finished");
#endif
    }

    pair<vector<int64_t>, vector<wstring>> toTokens(wstring str) {
        vector <int64_t> outTokens;
        vector <wstring> outStrings;

        outStrings.push_back(startToken);
        outTokens.push_back(str2tok[startToken]); //out.push_back(str2tok[L"<s>"]);

        while (str.size() > 0) {
            int j = (int)str.size();
            while (j > 0) {
                wstring tmp = str.substr(0, j);
                int id;
                if (str2tok.count(tmp)) {
                    id = str2tok[tmp];
                    outStrings.push_back(tmp);
                    outTokens.push_back(id);
                    // wprintf(L"%i (j=%i)\n", id, j);
                    break;
                }
                else {
                    j--;
                }
            }
            if (j == 0) {
                wstring tmpOutString = str.substr(0, 1);
                outStrings.push_back(tmpOutString);
                outTokens.push_back(str2tok[unkToken]);
                j = 1;
                //break;
            }

            str = str.substr(j);
        }

        outStrings.push_back(stopToken);
        outTokens.push_back(str2tok[stopToken]);
        //getchar();

        return make_pair(outTokens, outStrings);
    }
};

class NeMoPC {
public:
    Ort::Env env;
    Ort::SessionOptions options;
    unique_ptr<Ort::Session> model;

    NeMoPC_Tokenizer tok;
#if defined(__ANDROID__)
    std::locale myLocale = std::locale("C.UTF-8");
#elif defined(_WIN32)
    std::locale myLocale = std::locale("en_US.UTF-8");
#endif

    bool useTypeIds;    // if PnC onnx model has "token_type_ids" as its input

    vector<wstring> punctSymbols;   // set in constructor
    bool useCapitalLetters;    // determines logits' shape -- if true, we have 2 logits for capitalization output tensor

#if defined(__ANDROID__)
    NeMoPC(AAssetManager* manager, string modelPath, string tokPath, TokenizerType type, vector<wstring> punctSymbols, bool useCapitalLetters, bool installTime) : tok(manager, tokPath, type, installTime), punctSymbols(punctSymbols), useCapitalLetters(useCapitalLetters) {
#elif defined(_WIN32)
        NeMoPC(wstring modelPath, wstring tokPath, TokenizerType type, vector<wstring> punctSymbols, bool useCapitalLetters) : tok(tokPath, type), punctSymbols(punctSymbols), useCapitalLetters(useCapitalLetters) {
#endif
        if (type == TokenizerType::RoBERTa)	// TODO: verify if this is so for all RoBERTa models (for slovene it was)
            useTypeIds = false;
        else
            useTypeIds = true;

        env = Ort::Env(
                OrtLoggingLevel::ORT_LOGGING_LEVEL_VERBOSE,//OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                "nemo_pc_inference");
        options.SetGraphOptimizationLevel(ORT_DISABLE_ALL);
        options.DisableCpuMemArena();

        vector<uint8_t> modelData;
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC: reading model");

        if (installTime)
            modelData = readAsset(manager, (char*)modelPath.c_str());
        else
            modelData = readFile(modelPath);
#elif defined(_WIN32)
        modelData = readFile(string(modelPath.begin(), modelPath.end()));
#endif

#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC: instantiating model (modelData.size = %i)", (int)modelData.size());
#endif
        try {
            model = make_unique<Ort::Session>(Ort::Session(env, modelData.data(), modelData.size(), options));
        }
        catch (const Ort::Exception& e) {
#if defined(__ANDROID__)
            __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "ORT Exception: %s", e.what());
#endif
        }

#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "NeMoPC: model ok");
#endif
    }

    int64_t dot(vector<int64_t> v) {
        int64_t out = 1;
        for (int64_t value : v)
            out *= value;
        return out;
    }

    wstring forward(wstring s) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "inside p&c forward for string %S\n", s.c_str());
#endif

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        // Inputs
        auto tokOut = tok.toTokens(s);
        vector<int64_t> tokens = tokOut.first;
        vector<wstring> strings = tokOut.second;

        //vector<int64_t> tokens = { 101, 3180, 3182, 3180, 3182, 7733, 102 };

        vector<int64_t> tokensShape = { 1, (int64_t)tokens.size() };
        vector<int64_t> attnMask = vector<int64_t>(tokens.size(), 1);
        vector<int64_t> attnMaskShape = tokensShape;
        vector<int64_t> typeIds = vector<int64_t>(tokens.size(), 0);
        vector<int64_t> typeIdsShape = tokensShape;
        //for (int64_t t : tokens)
        //    wcout << t << " ";
        //wcout << endl;

        vector<const char*> inputNames;
        if (useTypeIds)
            inputNames = { "input_ids", "attention_mask", "token_type_ids" };
        else
            inputNames = { "input_ids", "attention_mask" };

        vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, tokens.data(), tokens.size(), tokensShape.data(), tokensShape.size()));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, attnMask.data(), attnMask.size(), attnMaskShape.data(), attnMaskShape.size()));
        if (useTypeIds)
            inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, typeIds.data(), typeIds.size(), typeIdsShape.data(), typeIdsShape.size()));

        // Outputs
        vector<int64_t> punctLogitsShape;
        punctLogitsShape = { 1, (int64_t)tokens.size(), (int64_t)punctSymbols.size() };

        vector<float> punctLogits = vector<float>(dot(punctLogitsShape));

        vector<int64_t> capitLogitsShape = { 1, (int64_t)tokens.size(), useCapitalLetters ? 2 : 1 };
        vector<float> capitLogits = vector<float>(dot(capitLogitsShape));

        vector<const char*> outputNames = { "punct_logits", "capit_logits" };
        vector<Ort::Value> outputTensors;
        outputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, punctLogits.data(), punctLogits.size(), punctLogitsShape.data(), punctLogitsShape.size()));
        outputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, capitLogits.data(), capitLogits.size(), capitLogitsShape.data(), capitLogitsShape.size()));

        // Inference
        try {
            model->Run(Ort::RunOptions{ nullptr }, inputNames.data(), inputTensors.data(), inputTensors.size(), outputNames.data(), outputTensors.data(), outputTensors.size());
        }
        catch (Ort::Exception e) {
            // wprintf(L"%S\n", e.what());
            return L"";
        }

        // Process the output
        vector<wstring> punct;
        punct = punctSymbols;   // e.g. { L"", L"!", L",", L".", L"?" };

        vector<int> punctOut;
        for (int i = 0; i < punctLogitsShape[1]; i++) {
            float fMax = std::numeric_limits<float>::lowest();
            int jMax = 0;
            for (int j = 0; j < punctLogitsShape[2]; j++) {
                float logit = punctLogits[punctLogitsShape[2] * i + j];
                if (logit > fMax) {
                    fMax = logit;
                    jMax = j;
                }
            }
            punctOut.push_back(jMax);
            //wcout << jMax << " ";
        }

        vector<bool> capitOut;
        for (int i = 0; i < capitLogitsShape[1]; i++) {
            if (useCapitalLetters) {
                float logit0 = capitLogits[capitLogitsShape[2] * i];
                float logit1 = capitLogits[capitLogitsShape[2] * i + 1];

                bool capitalize = logit1 > logit0;
                capitOut.push_back(capitalize);
                //wcout << capitalize << " ";
            }
            else {
                capitOut.push_back(false);
            }
        }

        // Prepare output string
        // (code assumes that tokens are never succeeded by a space character)
        // TODO: if using logographic languages languages (e.g. Chinese), test thoroughly,
        //       because ' ' char may not be present (unless model includes English vocab).
        // Some relevant points:
        // - if using "Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0" from NeMo, it already predicts PnC, so we don't need additional PnC model
        // - For zh-CN, PnC available is NeMo's "punctuationcapitalization_zh_cn_bert_base_vdeployable_v3.0", however it doesn't predict punctuation well at all
        // - Korean language uses space symbols so this is OK

        wstring out = L"";
        int capitCandidateIndex = 0;
        for (int i = 1; i < tokens.size() - 1; i++) {
            if (strings[i].length() == 0)
                continue;

            if (strings[i][0] == L' ' && strings[i].length() > 1) {
                capitCandidateIndex = i;
            }

            if (capitOut[i] == true) {
                int nonSpacePos;
                if (capitCandidateIndex == 0)
                    nonSpacePos = 0;
                else
                    nonSpacePos = 1;

                strings[capitCandidateIndex][nonSpacePos] = std::toupper(strings[capitCandidateIndex][nonSpacePos], myLocale);
            }
        }

        for (int i = 1; i < tokens.size() - 1; i++) {
            if (strings[i].length() == 0)
                continue;

            if (strings[i][0] == L' ') {
                out += punct[punctOut[i - 1]];
            }

            out += strings[i];

            if (i == (tokens.size() - 2))
                out += punct[punctOut[i]];
        }

        return out;
    }

    int computeNumCharsMemory(wstring strNew, int numTokensMemory) {
        int maxStrLen = 200;
        int truncStrLen = std::min((int)strNew.length(), maxStrLen);
        wstring substrNew = strNew.substr((int)strNew.length() - truncStrLen);
        auto tokOut = tok.toTokens(substrNew);

        int numSubstrTokens = tokOut.first.size();
        int substrLength = substrNew.length();
        // if we e.g. have 10 tokens and 70 length of string, and numTokensMemory=5
        // it means 5 tokens on average correspond to 5*70/10 = 35 characters
        int numChars = (int)floor((float)numTokensMemory * (float)substrLength / (float)numSubstrTokens);
        return numChars;
    }

    /*
    wstring punctuateAndCapitalize(wstring& strPrev, wstring& strNew) {
        // strPrev *is* punctuated & capitalized     (for example: "Hello, Slovenia is a beautiful ")
        // strNew is not punctuated & capitalized    (for example: "hello slovenia is a beautiful country and")

        int numTokensMemory = 10;
        int numCharsMemory = computeNumCharsMemory(strNew, numTokensMemory);
        wcout << "numCharsMemory = " << numCharsMemory << endl;
        //int numCharsMemory = 50;    // smaller value means faster inference

        if (!strNew.length())
            return L"";

        if (!strPrev.length())
            return this->forward(strNew);

        int overlapEndIndex = 0;
        for (int i = 0; i < std::min(strPrev.size(), strNew.size()); i++) {
            overlapEndIndex = i;

            if (strPrev[i] != strNew[i])
                break;
        }

        int cutIndex = std::max(overlapEndIndex - numCharsMemory, 0);	// wtNew words from this index onward will be used during inference
        int indexDiff = overlapEndIndex - cutIndex;

        wprintf(L"strPrev# = %i, strNew# = %i, overlapEndIndex = %i, cutIndex = %i, indexDiff = %i\n", strPrev.size(), strNew.size(), overlapEndIndex, cutIndex, indexDiff);

        wstring strNewPC = this->forward(strNew.substr(cutIndex));	// (numCharsMemory + new text), punctuated and capitalized
        wprintf(L"strNewPC = \"%s\"\n", strNewPC.c_str());

        wstring strOut;

        int strFirstLen = overlapEndIndex;
        int strSecondLen = strNewPC.length() - indexDiff;

        wstring strFirst = strPrev.substr(0, strFirstLen);
        wstring strSecond = strNewPC.substr(indexDiff, strSecondLen);

        // merge two strings
        strOut = strFirst + strSecond;

        return strOut;
    }*/

    bool isPunctuationSymbol(wchar_t c) {
        bool out = false;
        for (int i = 0; i < punctSymbols.size(); i++) {
            if (punctSymbols[i].size()) {
                wchar_t sym = punctSymbols[i][0];
                if (c == sym) {
                    out = true;
                    break;
                }
            }
        }
        return out;
    }

    wstring normalize(wstring strPC) {
        wstring out;
        for (int i = 0; i < strPC.length(); i++) {
            if (isPunctuationSymbol(strPC[i]))
                continue;
            else
                out += std::tolower(strPC[i], myLocale);
        }
        return out;
    }

    wstring punctuateAndCapitalize(wstring & strPrev, wstring & strNew) {
        // strPrev *is* punctuated & capitalized     (for example: "Hello, Slovenia is a beautiful ")
        // strNew is not punctuated & capitalized    (for example: "hello slovenia is a beautiful country and")

        int numTokensMemory = 10;
        int numCharsMemory = computeNumCharsMemory(strNew, numTokensMemory);

        if (!strNew.length())
            return L"";

        if (!strPrev.length())
            return this->forward(strNew);

        wstring strPrevNoPunct = normalize(strPrev);
        int numEqual = 0;
        for (int i = 0; i < strPrevNoPunct.length() && i < strNew.length(); i++) {
            if (strNew[i] != strPrevNoPunct[i])
                break;
            numEqual++;
        }

        int from = std::max(0, numEqual - numCharsMemory);
        wstring strInput = strNew.substr(from); // characters strNew[>=from] will be copied
        wstring strInputPC = this->forward(strInput);	// (numCharsMemory + new text), punctuated and capitalized

        int toStrFirst = 0;
        for (int i = 0; i < from && toStrFirst < strPrev.length();) {
            if (!isPunctuationSymbol(strPrev[toStrFirst]))
                i++;
            toStrFirst++;
        }

        wstring strFirst = strPrev.substr(0, toStrFirst);
        wstring strSecond = strInputPC;
        wstring strOut = strFirst + strSecond;

        return strOut;
    }
};
