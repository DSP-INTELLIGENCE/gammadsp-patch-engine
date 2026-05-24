// RBJTVSVFCore.hpp
#pragma once
#include <cmath>

// ================================================================
// TVSVF core (state + processing)
// ================================================================

struct TVSVFState
{
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float g   = 0.0f;
    float k   = 1.0f;
    float mHP = 0.0f;
    float mBP = 0.0f;
    float mLP = 1.0f;
};

class TVSVF
{
public:
    static inline float processSample(TVSVFState& s, float x)
    {
        float a  = 1.0f / (1.0f + s.g * (s.g + s.k));
        float v1 = a * (s.ic1 + s.g * (x - s.ic2));
        float v2 = s.ic2 + s.g * v1;

        float hp = x - s.k * v1 - v2;
        float bp = v1;
        float lp = v2;

        float y = s.mHP * hp + s.mBP * bp + s.mLP * lp;

        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;

        return y;
    }

    static inline void reset(TVSVFState& s)
    {
        s.ic1 = 0.0f;
        s.ic2 = 0.0f;
    }
};

// ================================================================
// RBJ definitions
// ================================================================

enum RBJFilterType
{
    RBJ_LPF = 0,
    RBJ_HPF,
    RBJ_BPF_SKIRT,
    RBJ_BPF_0DB,
    RBJ_NOTCH,
    RBJ_ALLPASS,
    RBJ_PEAKING,
    RBJ_LOWSHELF,
    RBJ_HIGHSHELF
};

enum RBJAlphaMode
{
    ALPHA_MODE_Q = 0,
    ALPHA_MODE_BW,
    ALPHA_MODE_S
};

struct RBJParams
{
    float sampleRate = 44100.0f;
    float cutoff     = 1000.0f;
    float Q          = 0.707f;
    float gainDB     = 0.0f;
    float BW         = 1.0f;
    float S          = 1.0f;
    int   type       = RBJ_LPF;
    int   alphaMode  = ALPHA_MODE_Q;
    float smoothing  = 1.0f;
};

class RBJ
{
public:
    static void computeCoeffs(const RBJParams& p,
                              float& b0, float& b1, float& b2,
                              float& a0, float& a1, float& a2);
};

// ================================================================
// RBJ → TVSVF glue engine
// ================================================================

class RBJTVSVF
{
public:
    RBJTVSVF() = default;

    inline void reset()
    {
        TVSVF::reset(state);
    }

    inline float process(float x, const RBJParams& params)
    {
        updateParameters(params);
        return TVSVF::processSample(state, x);
    }

private:
    TVSVFState state;

    void updateParameters(const RBJParams& p);

    static void denomToGK(float a1, float a2,
                          float& g, float& k, float& A0);

    static void numerToMix(float b0, float b1, float b2,
                           float g, float A0,
                           float& mHP, float& mBP, float& mLP);

    static inline float smooth(float prev, float target, float a)
    {
        return prev + a * (target - prev);
    }
};
