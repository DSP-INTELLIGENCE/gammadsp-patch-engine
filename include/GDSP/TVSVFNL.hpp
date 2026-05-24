// TVSVFNL.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include "SVF.hpp"

// Nonlinear (tanh) variant of TVSVF.
// One-shot nonlinearity: saturate the feedback state (s1) only.
// Keeps the ZDF/TPT structure intact and remains modulation-safe.
class TVSVFNL
{
public:
    TVSVFNL();

    // legacy-style API
    void setSampleRate(float fs);
    void setType(SVFType type);
    void setCutoff(float hz);
    void setQ(float q);

    // nonlinear control
    void setDrive(float drive); // typical range: [1..20] (clamped)

    // canonical injection (for RBJSVF-like driving)
    void setCoeffs(float g, float k, float mHP, float mBP, float mLP);

    float process(float x, int channel);
    void reset();


    void updateCoeffs();

    // user parameters
    SVFType type = SVFLowpass;
    float sampleRate = 44100.0f;
    float cutoff = 1000.0f;
    float Q = 0.707f;
    float drive = 1.0f;

    // canonical coefficients (TVSVF contract)
    float g = 0.0f;
    float k = 1.0f;
    float mHP = 0.0f, mBP = 0.0f, mLP = 1.0f;

    // ZDF state
    struct State { float s1 = 0.0f, s2 = 0.0f; };
    State state[2];
};
