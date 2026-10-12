// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#define __STDC_WANT_LIB_EXT1__ 1

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cerrno>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/log.h>
#endif

using namespace std;

#ifndef _MSC_VER
inline int fopen_s(FILE** f, const char* name, const char* mode) {
    if (!f || !name || !mode) {
        return EINVAL;
    }
    *f = fopen(name, mode);
    return (*f != nullptr) ? 0 : errno;
}
#endif

vector<uint8_t> readFile(string filePath) {
    vector<uint8_t> ret;

    ifstream f(filePath, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        return ret;

    streamsize size = f.tellg();
    ret = vector<uint8_t>(size);

    f.seekg(std::ios::beg);
    f.read((char*)ret.data(), size);
    f.close();

    return ret;
}

#if defined(__ANDROID__)
vector<uint8_t> readAsset(AAssetManager* manager, char* assetFileName) {
    __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "Opening asset with name %s", assetFileName);
    AAsset* asset = AAssetManager_open(manager, assetFileName, AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "asset is NULL");
        return {};
    }
    else
        __android_log_print(ANDROID_LOG_INFO, "UHO_Captions", "asset is OK");

    uint8_t* buffer = (uint8_t*)AAsset_getBuffer(asset);
    long len = AAsset_getLength(asset);
    vector<uint8_t> v(buffer, buffer + len);
    AAsset_close(asset);
    return v;
}
#endif


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
}
