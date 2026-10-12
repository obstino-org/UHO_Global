// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

float DTW(vector<float>& x, vector<float>& y) {
	//float** D;
    int N = x.size();
	int M = y.size();

	if (!N || !M)
		return std::numeric_limits<float>::max();

    // Allocate
    vector<vector<float>> D;
    D.resize(N + 1);
    for (vector<float>& v : D)
        v.resize(M + 1);

    //D = (float**)calloc(N + 1, sizeof(float*));
	//for (int k = 0; k < N + 1; k++)
	//	D[k] = (float*)calloc(M + 1, sizeof(float));

	D[0][0] = 0.0f;
	for (int k = 1; k < M + 1; k++)
		D[0][k] = numeric_limits<float>::infinity();
	for (int j = 1; j < N + 1; j++)
		D[j][0] = numeric_limits<float>::infinity();

	for (int j = 1; j < N + 1; j++) {
		for (int k = 1; k < M + 1; k++) {
			D[j][k] = abs(x[j - 1] - y[k - 1]) + min(min(D[j - 1][k - 1], D[j - 1][k]), D[j][k - 1]);
		}
	}

	// Cleanup
	//for (int k = 0; k < N; k++)
	//	free(D[k]);
	//free(D);

	return D[N][M];
}
