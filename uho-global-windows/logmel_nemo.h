// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once
#define _USE_MATH_DEFINES
#include <iostream>
#include <vector>
#include <math.h>
#include "kiss_fftr.h"
#include "Eigen/Dense"
#include "common_io.h"
#undef min
#undef max
using namespace std;

#define N_FFT	512
#define WIN_LEN	400
#define HOP_LEN	160

class LogMelNeMo {
public:
    Eigen::MatrixXf melFilterMatrix;
    int numFilters;

#if defined(__ANDROID__)
    LogMelNeMo(AAssetManager* manager, string melFiltersPath = "melfilters.bin", int numFilters = 80) {
#elif defined(_WIN32)
    LogMelNeMo(wstring melFiltersPath = L"melfilters.bin", int numFilters = 80) {
#endif
        this->numFilters = numFilters;

#if defined(__ANDROID__)
        readMelFilters(manager, melFiltersPath, melFilterMatrix);
#elif defined(_WIN32)
        readMelFilters(melFiltersPath, melFilterMatrix);
#endif
        // melscale_fbanks shape is (257, numFilters)
    }

    // (!) MUST (!) return M by reference, because otherwise by returning matrix we get an exception
    // (issue is apparently related to Eigen's SIMD operations alignment)
#if defined(__ANDROID__)
    void readMelFilters(AAssetManager * manager, string path, Eigen::MatrixXf & M) {
#elif defined(_WIN32)
    void readMelFilters(wstring path, Eigen::MatrixXf & M) {
#endif

        M.resize(257, numFilters);

#if defined(__ANDROID__)
        vector<uint8_t> buff = readAsset(manager, (char*)path.c_str());
#elif defined(_WIN32)
        vector<uint8_t> buff = readFile(string(path.begin(), path.end()));
#endif

        if (buff.empty())
            return;

        for (int i = 0; i < M.rows() * M.cols(); i++) {
            float u;
            u = *((float*)&buff[i * sizeof(float)]);

            // todo check for sure if its correct
            int row = (int)(i / M.cols());
            int col = i - row * M.cols();
            M(row, col) = u;
        }
    }

    vector<vector<kiss_fft_cpx>> computeSTFT(vector<float>&x)
    {
        vector<vector<kiss_fft_cpx>> output;

        kiss_fftr_cfg config;
        config = kiss_fftr_alloc(N_FFT, 0, NULL, NULL);

        for (int i = 0; i < x.size(); i += HOP_LEN) {
            // obtain audio frame
            int from = i;
            int to = std::min(from + WIN_LEN, (int)x.size());
            vector<float> frame = vector<float>(x.begin() + from, x.begin() + to);
            frame.resize(WIN_LEN);

            // windowing
            for (int j = 0; j < frame.size(); j++)
                frame[j] *= 1.0 / 2.0 * (1.0 - cos(2 * M_PI * (float)j / ((float)WIN_LEN - 1.0)));

            // pad end of frame
            frame.resize(N_FFT);

            kiss_fft_cpx* out = (kiss_fft_cpx*)calloc(frame.size(), sizeof(kiss_fft_cpx));
            kiss_fftr(config, frame.data(), out);

            vector<kiss_fft_cpx> v_out;
            for (int j = 0; j < N_FFT / 2 + 1 /*frame.size()*/; j++)
                v_out.push_back(out[j]);
            output.push_back(v_out);
            kiss_fftr_free(out);
        }

        kiss_fftr_free(config);
        return output;
    }

    vector <vector<float>> computeMagnitudeSquared(vector<vector<kiss_fft_cpx>>&stft) {
        vector <vector<float>> out;
        if (!stft.size())
            return out;

        for (int i = 0; i < stft.size(); i++) {
            vector<float> tmp;
            for (int j = 0; j < stft[0].size(); j++) {
                float m = pow(stft[i][j].r, 2.0f) + pow(stft[i][j].i, 2.0f);
                tmp.push_back(m);
            }
            out.push_back(tmp);
        }
        return out;
    }

    vector<vector<float>> transpose(vector<vector<float>>&mel)
    {
        vector<vector<float>> mel_T;
        if (!mel.size())
            return mel_T;

        mel_T = vector<vector<float>>(mel[0].size());
        for (vector<float>& v : mel_T)
            v = vector<float>(mel.size());

        for (int i = 0; i < mel.size(); i++) {
            for (int j = 0; j < mel[0].size(); j++) {
                mel_T[j][i] = mel[i][j];
            }
        }

        // cout << mel_T.size() << "; " << mel_T[0].size() << endl;  // example output: 80; 3000

        return mel_T;
    }

    vector<float> flatten(vector<vector<float>> v)
    {
        vector<float> out;
        if (!v.size())
            return out;

        for (int i = 0; i < v.size(); i++) {
            for (int j = 0; j < v[0].size(); j++) {
                out.push_back(v[i][j]);
            }
        }

        return out;
    }

    vector<float> flatten(Eigen::MatrixXf v)
    {
        vector<float> out;
        if (!v.size())
            return out;

        for (int i = 0; i < v.rows(); i++) {
            for (int j = 0; j < v.cols(); j++) {
                out.push_back(v(i, j));
            }
        }

        return out;
    }

    void toMatrix(vector <vector<float>> v, Eigen::MatrixXf & out) {
        if (!v.size())
            return;

        out.resize(v.size(), v[0].size());
        for (int i = 0; i < v.size(); i++) {
            for (int j = 0; j < v[0].size(); j++)
                out(i, j) = v[i][j];
        }
    }

    void normalize(Eigen::MatrixXf mat, Eigen::MatrixXf & mels) {
        mels = mat;

        int n_mel = mat.cols();
        int n_time = mat.rows();

        vector<float> mean(n_mel);
        vector<float> std(n_mel);

        for (int i = 0; i < n_mel; i++) {
            for (int j = 0; j < n_time; j++)
                mean[i] += mels(j, i);

            mean[i] /= (float)n_time;
        }

        for (int i = 0; i < n_mel; i++) {
            for (int j = 0; j < n_time; j++)
                std[i] += pow(mels(j, i) - mean[i], 2.0f);

            std[i] /= (float)n_time;
            std[i] = sqrt(std[i]);
        }

        for (int i = 0; i < n_mel; i++) {
            for (int j = 0; j < n_time; j++) {
                mels(j, i) = (mels(j, i) - mean[i]) / (std[i] + 1.0e-5f);
            }
        }

        //return mels;
    }

    // Based on the approach taken by https://github.com/istupakov/onnx-asr/blob/main/preprocessors/nemo.py
    vector<float> computeLogMel(vector<float>&x, bool normalizeFeatures = true, bool transpose = true)
    {
        float logGuardConst = pow(2.0f, -24);
        vector<vector<kiss_fft_cpx>> stft1 = computeSTFT(x);
        //wcout << stft1.size() << ", " << stft1[0].size() << endl;
        vector <vector<float>> stft2 = computeMagnitudeSquared(stft1);
        //wcout << stft2.size() << ", " << stft2[0].size() << endl;
        Eigen::MatrixXf spectrumMatrix;
        toMatrix(stft2, spectrumMatrix);
        //wcout << spectrumMatrix.rows() << L" x " << spectrumMatrix.cols() << endl;

        // Multiply matrices of shapes (N_time, 257) x (257, numFilters) to obtain matrix outMul of shape (N_time, numFilters)
        // Here 257 is n_fft/2 + 1, and numFilters is number of mel filters
        Eigen::MatrixXf outMul = spectrumMatrix * melFilterMatrix;

        //wcout << outMul.rows() << L" x " << outMul.cols() << endl;
        for (int i = 0; i < outMul.rows(); i++)
        {
            for (int j = 0; j < outMul.cols(); j++) {
                outMul(i, j) = log(outMul(i, j) + logGuardConst);
            }
        }

        Eigen::MatrixXf norm;
        if (normalizeFeatures)
            normalize(outMul, norm);
        else
            norm = outMul;

        vector<float> output;
        if (transpose)
            output = flatten(norm.transpose());
        else
            output = flatten(norm);

        return output;
    }
};
