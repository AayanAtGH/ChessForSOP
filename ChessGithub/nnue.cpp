#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>



//This happens because bbc.cpp includes nnue.cpp at line 7, so the constexpr int white = 0 in nnue.cpp gets pasted in first, then bbc.cpp defines enum {white, black, both} — two definitions of white in the same translation unit.
//The cleanest fix without restructuring anything is to just use the raw integer values directly in nnue.cpp instead of the named constants:

// nnue.cpp — use raw values, no named constants needed
extern int side;
extern int enpassant;
extern int castle;

/*
// In EvaluateNNUE, use numbers directly:
if (side == 0)          l1[i] += FeatureWeights[768][i];  // 0 = white
if (castle & 1)         l1[i] += FeatureWeights[769][i];  // 1 = wk
if (castle & 2)         l1[i] += FeatureWeights[770][i];  // 2 = wq
if (castle & 4)         l1[i] += FeatureWeights[771][i];  // 4 = bk
if (castle & 8)         l1[i] += FeatureWeights[772][i];  // 8 = bq
if (enpassant != 64)    l1[i] += FeatureWeights[773 + enpassant % 8][i];  // 64 = no_sq
*/




// --- Network Dimensions ---
constexpr int INPUT_SIZE = /*768*/781; //changed to 781 to account for sidetomove, castling, enpassant
constexpr int L1_SIZE = 512;
constexpr int L2_SIZE = /*32*/ 128;  //NOW 128 instead of 32
constexpr int L3_SIZE = 32;

// --- Network Weights & Biases ---
// We define these globally (or in a struct/namespace)
float FeatureWeights[INPUT_SIZE][L1_SIZE];
float FeatureBiases[L1_SIZE];

float L2Weights[L1_SIZE][L2_SIZE];
float L2Biases[L2_SIZE];

float L3Weights[L2_SIZE][L3_SIZE];
float L3Biases[L3_SIZE];

float OutputWeights[L3_SIZE]; // Shape is [32][1], so it's a 1D array
float OutputBias;

// --- Initialization Function ---
// Call this ONCE when your engine boots up in main()
bool LoadNNUE(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to load NNUE file: " << filepath << std::endl;
        return false;
    }

    // Read FC1 (768x512 weights + 512 biases)  //now 781 isntead of 768
    file.read(reinterpret_cast<char*>(FeatureWeights), sizeof(FeatureWeights));
    file.read(reinterpret_cast<char*>(FeatureBiases), sizeof(FeatureBiases));

    // Read FC2 (512x32 weights + 32 biases)  //128 isntead of 32
    file.read(reinterpret_cast<char*>(L2Weights), sizeof(L2Weights));
    file.read(reinterpret_cast<char*>(L2Biases), sizeof(L2Biases));

    // Read FC3 (32x32 weights + 32 biases)
    file.read(reinterpret_cast<char*>(L3Weights), sizeof(L3Weights));
    file.read(reinterpret_cast<char*>(L3Biases), sizeof(L3Biases));

    // Read FC4 (32x1 weights + 1 bias)
    file.read(reinterpret_cast<char*>(OutputWeights), sizeof(OutputWeights));
    file.read(reinterpret_cast<char*>(&OutputBias), sizeof(OutputBias));

    file.close();
    std::cout << "Successfully loaded NNUE: " << filepath << std::endl;
    return true;
}

// --- The Accumulator Structure ---
struct Accumulator {
    float values[L1_SIZE];
    
    void copy_from(const Accumulator& parent) {
        std::copy(std::begin(parent.values), std::end(parent.values), std::begin(values));
    }
};

// Clipped ReLU Activation
inline float clipped_relu(float x) {
    return std::max(0.0f, std::min(1.0f, x));
}

// --- Evaluation Function ---
// Pass your Search Stack's current Accumulator here
float EvaluateNNUE(const Accumulator& acc) {



    float l1[L1_SIZE];
    
    // Start from accumulator (piece features 0-767, maintained incrementally)
    // Then add features 768-780 fresh from current board state
    for (int i = 0; i < L1_SIZE; i++) {
        l1[i] = acc.values[i];

        // feature 768: side to move
        if (side == 0)  l1[i] += FeatureWeights[768][i];

        // features 769-772: castling rights
        if (castle & 1)    l1[i] += FeatureWeights[769][i];
        if (castle & 2)    l1[i] += FeatureWeights[770][i];
        if (castle & 4)    l1[i] += FeatureWeights[771][i];
        if (castle & 8)    l1[i] += FeatureWeights[772][i];

        // features 773-780: en passant file
        if (enpassant != 64)   l1[i] += FeatureWeights[773 + enpassant % 8][i];
    }



    // ADD HERE
    static bool printed = false;
    if (!printed) {
        printf("First 5 l1 values (pre-clamp):\n");
        printf("%.6f %.6f %.6f %.6f %.6f\n",
            l1[0], l1[1], l1[2], l1[3], l1[4]);
        printed = true;
    }




    float layer2_out[L2_SIZE] = {0};
    float layer3_out[L3_SIZE] = {0};
    
    // Layer 1 (Accumulator) -> Layer 2
    for (int j = 0; j < L2_SIZE; ++j) {
        layer2_out[j] = L2Biases[j];
        for (int i = 0; i < L1_SIZE; ++i) {
            layer2_out[j] += clipped_relu(l1[i]/*acc.values[i]*/) * L2Weights[i][j];     //CLAUDE: rep acc.values[i] by l1[i]
        }
        layer2_out[j] = clipped_relu(layer2_out[j]); 
    }
    
    // Layer 2 -> Layer 3
    for (int j = 0; j < L3_SIZE; ++j) {
        layer3_out[j] = L3Biases[j];
        for (int i = 0; i < L2_SIZE; ++i) {
            layer3_out[j] += layer2_out[i] * L3Weights[i][j];
        }
        layer3_out[j] = clipped_relu(layer3_out[j]); 
    }
    
    // Layer 3 -> Output
    float final_eval = OutputBias;
    for (int i = 0; i < L3_SIZE; ++i) {
        final_eval += layer3_out[i] * OutputWeights[i];
    }
    
    // final_eval is a Win Probability (0.0 to 1.0). 
    // Convert it back to centipawns for the engine's Alpha-Beta bounds.
    if (final_eval <= 0.001f) final_eval = 0.001f;
    if (final_eval >= 0.999f) final_eval = 0.999f;
    
    // Inverse sigmoid function: eval = -400 * ln((1/p) - 1)
    float centipawns = -400.0f * std::log((1.0f / final_eval) - 1.0f);
    
    return centipawns;
}