// RBJTVSVF.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "RBJSVF.hpp"

// ------------------------------------------------------------
// RBJTVSVF class
// ------------------------------------------------------------

class RBJTVSVF
{
public:
    
    RBJTVSVF(float fs = 48000.0f);

    void setSampleRate(float fs);
    void reset();

    void setType(RBJType t);
    void setAlphaMode(RBJAlphaMode m);

    void setCutoff(float hz);
    void setQ(float q);
    void setGainDB(float db);
    void setBandwidth(float bw);
    void setShelfSlope(float s);

    void setSmoothing(float a); // 0..1

    float process(float x);
    void processBlock(const float* in, float* out, int n);

private:
    // --------------------------------------------------------
    // User parameters
    // --------------------------------------------------------

    float fs = 48000.0f;
    float f0 = 1000.0f;
    float Q = 0.707f;
    float dBgain = 0.0f;
    float BW = 1.0f;
    float S = 1.0f;

    RBJType type = RBJ_LPF;
    RBJAlphaMode alphaMode = ALPHA_MODE_Q;

    float smoothA = 1.0f;

    // --------------------------------------------------------
    // RBJ biquad coefficients (normalized)
    // --------------------------------------------------------

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    // --------------------------------------------------------
    // SVF mapped coefficients
    // --------------------------------------------------------

    float g = 0.0f, k = 0.0f, A0 = 1.0f;
    float mHP = 0.0f, mBP = 0.0f, mLP = 1.0f;

    // Smoothed versions
    float gS = 0.0f, kS = 1.0f;
    float mHPS = 0.0f, mBPS = 0.0f, mLPS = 1.0f;

    // --------------------------------------------------------
    // ZDF SVF state
    // --------------------------------------------------------

    float ic1 = 0.0f;
    float ic2 = 0.0f;

    // --------------------------------------------------------
    // Internal helpers
    // --------------------------------------------------------

    void updateRBJ();
    void updateMapping();
    void smoothParams();

    float computeAlpha(float w0, float sw, float A);
    void denomToGK();
    void numerToMix();

    inline float tickSVF(float x);

    static inline float clamp(float x, float lo, float hi);
};
