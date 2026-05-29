#pragma once
#include <cmath>
#include <algorithm>
#include "TVSVF.hpp"

// ------------------------------------------------------------
// RBJ filter types
// ------------------------------------------------------------
enum RBJType {
    RBJ_LPF,
    RBJ_HPF,
    RBJ_BPF_SKIRT,
    RBJ_BPF_0DB,
    RBJ_NOTCH,
    RBJ_ALLPASS,
    RBJ_PEAKING,
    RBJ_LOWSHELF,
    RBJ_HIGHSHELF
};

// ------------------------------------------------------------
// RBJ alpha interpretation modes
// ------------------------------------------------------------
enum RBJAlphaMode {
    ALPHA_MODE_Q,
    ALPHA_MODE_BW,
    ALPHA_MODE_S
};

class RBJSVF {
public:
    
// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

void setSampleRate(float v) { fs = v; updateRBJ(); updateMapping(); }
void setType(RBJType t)     { type = t; updateRBJ(); updateMapping(); }
void setCutoff(float v)     { f0 = v; updateRBJ(); updateMapping(); }
void setQ(float v)          { Q = v; updateRBJ(); updateMapping(); }
void setGainDB(float v)     { dBgain = v; updateRBJ(); updateMapping(); }
void setBandwidth(float v)  { BW = v; updateRBJ(); updateMapping(); }
void setShelfSlope(float v) { S = v; updateRBJ(); updateMapping(); }
void setAlphaMode(RBJAlphaMode m) { alphaMode = m; updateRBJ(); updateMapping(); }

void reset() { svf.reset(); }

float process(float x)
{
    svf.setCoeffs(g, k, mHP, mBP, mLP);
    return svf.process(x, 0);
}

// ------------------------------------------------------------
// RBJ coefficient generator
// ------------------------------------------------------------

static inline float clampf(float x, float lo, float hi)
{
    return (x < lo) ? lo : (x > hi ? hi : x);
}

void updateRBJ()
{
    fs = clampf(fs, 1.0f, 768000.0f);
    f0 = clampf(f0, 0.0f, 0.49f * fs);
    Q  = clampf(Q, 1e-5f, 1e6f);
    BW = clampf(BW, 1e-5f, 24.0f);
    S  = clampf(S, 1e-5f, 10.0f);
    dBgain = clampf(dBgain, -120.0f, 120.0f);

    const float A  = std::pow(10.0f, dBgain / 40.0f);
    const float w0 = 2.0f * float(M_PI) * (f0 / fs);
    const float cw = std::cos(w0);
    const float sw = std::sin(w0);

    float alpha;
    if (alphaMode == ALPHA_MODE_Q)
        alpha = sw / (2.0f * Q);
    else if (alphaMode == ALPHA_MODE_BW)
        alpha = sw * std::sinh((std::log(2.0f) / 2.0f) * BW * w0 / sw);
    else
        alpha = (sw / 2.0f) * std::sqrt((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);

    float b0_, b1_, b2_, a0_, a1_, a2_;

    switch (type) {
        case RBJ_LPF:
            b0_ = (1 - cw) * 0.5f; b1_ = 1 - cw; b2_ = (1 - cw) * 0.5f;
            a0_ = 1 + alpha; a1_ = -2 * cw; a2_ = 1 - alpha; break;

        case RBJ_HPF:
            b0_ = (1 + cw) * 0.5f; b1_ = -(1 + cw); b2_ = (1 + cw) * 0.5f;
            a0_ = 1 + alpha; a1_ = -2 * cw; a2_ = 1 - alpha; break;

        case RBJ_NOTCH:
            b0_ = 1; b1_ = -2 * cw; b2_ = 1;
            a0_ = 1 + alpha; a1_ = -2 * cw; a2_ = 1 - alpha; break;

        default:
            b0_ = 1; b1_ = 0; b2_ = 0;
            a0_ = 1; a1_ = 0; a2_ = 0;
    }

    b0 = b0_ / a0_;
    b1 = b1_ / a0_;
    b2 = b2_ / a0_;
    a1 = a1_ / a0_;
    a2 = a2_ / a0_;
}

// ------------------------------------------------------------
// RBJ → ZDF SVF mapping
// ------------------------------------------------------------

void updateMapping()
{
    float den = 1 + a2 - a1;
    if (std::abs(den) < 1e-9f) den = 1e-9f;

    const float A0 = 4.0f / den;
    float g2 = 1 + 0.5f * a1 * A0;
    g = std::sqrt(std::max(g2, 1e-12f));
    k = std::max((A0 * (1 - a2)) / (2 * g), 0.0f);

    const float B0 = b0 * A0;
    const float B1 = b1 * A0;
    const float B2 = b2 * A0;

    const float S = 0.5f * (B0 + B2);
    const float T = 0.5f * B1;

    mBP = (B0 - B2) / (2 * g);
    mLP = (S + T) / (2 * g * g);
    mHP = S - mLP * g * g;
}

    
    // user parameters
    float fs = 48000.0f;
    float f0 = 1000.0f;
    float Q = 0.707f;
    float dBgain = 0.0f;
    float BW = 1.0f;
    float S = 1.0f;

    RBJType type = RBJ_LPF;
    RBJAlphaMode alphaMode = ALPHA_MODE_Q;

    // RBJ biquad coefficients (normalized)
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;


    // SVF-mapped coefficients
    float g = 0.0f;
    float k = 0.0f;
    float mHP = 0.0f;
    float mBP = 0.0f;
    float mLP = 0.0f;

    TVSVF svf;

    void updateRBJ();
    void updateMapping();    
};
