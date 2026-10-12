// Levenshtein distance by https://github.com/guilhermeagostinelli/levenshtein/blob/master/levenshtein.cpp
// (slightly modified to normalize distance)

#pragma once
#include <iostream>
#include <vector>
using namespace std;

// Returns the Levenshtein distance between word1 and word2.
float levenshteinDist(wstring word1, wstring word2) {
    int size1 = word1.size();
    int size2 = word2.size();

    if (!size1 || !size2)
        return std::numeric_limits<float>::max();

    //int verif[size1 + 1][size2 + 1]; // Verification matrix i.e. 2D array which will store the calculated distance.
    vector<vector<int>> verif;
    verif.resize(size1 + 1);
    for (vector<int>& v : verif)
        v.resize(size2 + 1);

    // If one of the words has zero length, the distance is equal to the size of the other word.
    if (size1 == 0)
        return size2;
    if (size2 == 0)
        return size1;

    // Sets the first row and the first column of the verification matrix with the numerical order from 0 to the length of each word.
    for (int i = 0; i <= size1; i++)
        verif[i][0] = i;
    for (int j = 0; j <= size2; j++)
        verif[0][j] = j;

    // Verification step / matrix filling.
    for (int i = 1; i <= size1; i++) {
        for (int j = 1; j <= size2; j++) {
            // Sets the modification cost.
            // 0 means no modification (i.e. equal letters) and 1 means that a modification is needed (i.e. unequal letters).
            int cost = (word2[j - 1] == word1[i - 1]) ? 0 : 1;

            // Sets the current position of the matrix as the minimum value between a (deletion), b (insertion) and c (substitution).
            // a = the upper adjacent value plus 1: verif[i - 1][j] + 1
            // b = the left adjacent value plus 1: verif[i][j - 1] + 1
            // c = the upper left adjacent value plus the modification cost: verif[i - 1][j - 1] + cost
            verif[i][j] = min(
                min(verif[i - 1][j] + 1, verif[i][j - 1] + 1),
                verif[i - 1][j - 1] + cost
            );
        }
    }

    // The last position of the matrix will contain the Levenshtein distance.
    float d = (float)verif[size1][size2];
	
	// Add at least 1.0f to levDist to give it less weight
    // because otherwise levDist=0 could remove effect of dtwDist.
    // That was found to lead to errors in optimalShift being set to 0
    // and resulting word repeats might happen.
    float d_norm = (d + 1.0f) / (float)std::max(size1, size2);
    return d_norm;
    //return (float)verif[size1][size2];
}
