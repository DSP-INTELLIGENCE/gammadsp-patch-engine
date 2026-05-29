#pragma once
#include <algorithm>
#include <cmath>
#include "SVF.hpp"

class TVSVF: public Function
{
public:

    TVSVF();
    {
        updateCoeffs();
    }

    void setType(uint32_t t)
    {
        type = t;
        updateCoeffs();
    }

    void setCutoff(float hz)
    {
        cutoff = hz;
        updateCoeffs();
    }

    void setQ(float q)
    {
        Q = q;
        updateCoeffs();
    }

    void reset()
    {
        for (auto& s : state)
            s = {};
    }

    void setCoeffs(float newG, float newK, float hp, float bp, float lp)
    {
        g = newG;
        k = newK;
        mHP = hp;
        mBP = bp;
        mLP = lp;
    }

    void updateCoeffs()
    {
        cutoff = std::clamp(cutoff, 5.0f, 0.49f * gam::sampleRate());
        Q = std::max(Q, 0.05f);
        pCutoffParam.set(cutoff);
        pQParam.set(Q);
        g = std::tan(float(M_PI) * cutoff / gam::sampleRate());
        k = 1.0f / Q;

        switch (type)
        {
            case SVFLowpass:  mHP = 0;  mBP = 0;  mLP = 1;  break;
            case SVFBandpass: mHP = 0;  mBP = 1;  mLP = 0;  break;
            case SVFHighpass: mHP = 1;  mBP = 0;  mLP = 0;  break;
            case SVFNotch:    mHP = 1;  mBP = 0;  mLP = 1;  break;
            case SVFAllpass:  mHP = 1;  mBP = -k; mLP = 1;  break;
            case SVFPeak:     mHP = -1; mBP = 0;  mLP = 1;  break;
        }
    }

    void update()
    {
        cutoff = pCutoffParam.process();
        Q      = pQParam.process();
        cutoff = std::clamp(cutoff, 5.0f, 0.49f * sampleRate);
        Q = std::max(Q, 0.05f);
        g = std::tan(float(M_PI) * cutoff / sampleRate);
        k = 1.0f / Q;

    }

    float processChannel(float x, int ch)
    {
        auto& s = state[ch];

        update();
        const float den = std::max(1.0f + g * (g + k), 1.0e-12f);
        const float a = 1.0f / den;

        const float v1 = a * (s.s1 + g * (x - s.s2));
        const float v2 = s.s2 + g * v1;

        const float hp = x - k * v1 - v2;
        const float bp = v1;
        const float lp = v2;

        s.s1 = 2.0f * v1 - s.s1;
        s.s2 = 2.0f * v2 - s.s2;

        return mHP * hp + mBP * bp + mLP * lp;
    }


    float process(float x) override { return processChannel(x,0); }
    
    // user parameters
    SVFType type = SVFLowpass;
    float sampleRate = 44100.0f;
    float cutoff = 1000.0f;
    float Q = 0.707f;
            
    // canonical coefficients (this is the real TVSVF contract)
    float g = 0.0f;
    float k = 1.0f;
    float mHP = 0.0f, mBP = 0.0f, mLP = 1.0f;

private:
    // ZDF state
    struct State { float s1 = 0.0f, s2 = 0.0f; };
    State state[2];
};
