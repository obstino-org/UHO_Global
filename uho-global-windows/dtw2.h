// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
using namespace std;
#define NO_DEBUG

tuple<float, vector<vector<float>>, vector<pair<int, int>>> DTW2(vector<float>& x, vector<float>& y) {
	//float** D;
	int N = x.size();
	int M = y.size();

	if (!N || !M)
		return make_tuple(std::numeric_limits<float>::max(), vector<vector<float>>(), vector<pair<int, int>>());

	// Allocate
	vector<vector<float>> D;
	D.resize(N + 1);
	for (vector<float>& v : D)
		v.resize(M + 1);

#ifndef NO_DEBUG
	FILE* fd1;
	fopen_s(&fd1, "mat_D.dat", "w");
#endif
	
	//D = (float**)calloc(N + 1, sizeof(float*));
	//for (int k = 0; k < N + 1; k++)
	//	D[k] = (float*)calloc(M + 1, sizeof(float));

	D[0][0] = 0.0f;
	for (int k = 0; k < M + 1; k++)
		D[0][k] = numeric_limits<float>::infinity();
	for (int j = 0; j < N + 1; j++)
		D[j][0] = numeric_limits<float>::infinity();

	D[0][0] = 0.0f;

	for (int k = 1; k < M + 1; k++) {	// k (sortformer vad out) will be on x axis
		for (int j = 1; j < N + 1; j++) {	// j (asr vad out) will be on y axis
				D[j][k] = abs(x[j - 1] - y[k - 1]) + min(min(D[j - 1][k - 1], D[j - 1][k]), D[j][k - 1]);
#ifndef NO_DEBUG
			fprintf(fd1, "%.3f ", D[j][k]);
#endif
		}
#ifndef NO_DEBUG
		fprintf(fd1, "\n");
#endif
	}

#ifndef NO_DEBUG
	fclose(fd1);
#endif

	// Find the optimal path
	vector<pair<int, int>> path;
#ifndef NO_DEBUG
	FILE* fd2;
	fopen_s(&fd2, "matpath_D.dat", "w");
#endif

	for (int j = N, k = M; j > 0 || k > 0;)
	{
		path.push_back(make_pair(j - 1, k - 1));
		double d11 = std::numeric_limits<float>::infinity();
		double d10 = std::numeric_limits<float>::infinity();
		double d01 = std::numeric_limits<float>::infinity();

		if (j > 0 && k > 0) d11 = D[j - 1][k - 1];
		if (j > 0) d10 = D[j - 1][k];
		if (k > 0) d01 = D[j][k - 1];
		double m = std::min(std::min(d11, d10), d01);

		if (m == d11) {
			j--;
			k--;
		}
		else if (m == d10) {
			j--;
		}
		else if (m == d01) {
			k--;
		}
		
		/*
		path.push_back(pair<int, int>(j - 1, k - 1));
		float m;

		if (j > 1 && k > 1)
			m = min(min(D[j - 1][k - 1], D[j - 1][k]), D[j][k - 1]);
		else if (j > 1 && k == 1)
			m = D[j - 1][k];
		else if (j == 1 && k > 1)
			m = D[j][k - 1];

		if (j > 1 && k > 1 && m == D[j - 1][k - 1]) {
			j--;
			k--;
		}
		else if (j > 1 && m == D[j - 1][k]) {
			j--;
		}
		else if (k > 1) {
			k--;
		}
		*/
	}

	//path.push_back(pair<int, int>(0, 0));

#ifndef NO_DEBUG
	for (int k = 0; k < path.size(); k++)
		fprintf(fd2, "%i %i\n", path[k].first, path[k].second);
	fclose(fd2);
#endif

	float dist = D[N][M];
	vector<vector<float>> matrixOut;
	for (int i = 1; i <= N; i++) {
		matrixOut.push_back(vector<float>(D[i].begin() + 1, D[i].end()));
	}

	return make_tuple(dist, matrixOut, path);

	// Cleanup
	//for (int k = 0; k < N; k++)
	//	free(D[k]);
	//free(D);

	//return D[N][M];
}
