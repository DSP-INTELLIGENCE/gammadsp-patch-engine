#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"

class DFXChorus : public DFXCore
{
public:
    DFXChorus()
    : mDelay(0.05f, 0.02f) // 50ms max, 20ms init
    {
        mDelay.setIpolType(gam::ipl::Type::CUBIC);        
        setWet(1.0f);
        setDry(1.0f);
        setMix(0.35f);
    }

    
    void setRate(float hz)       { mRate = std::max(0.f, hz); }
    void setDepth(float ms)      { mDepth = std::max(0.f, ms * 0.001f); }
    void setBaseDelay(float ms)  { mBase  = std::max(0.0005f, ms * 0.001f); }

    float process(float x) override
    {
        // --- LFO (rounded triangle: more analog feel) ---
        float s = sinf(mPhase);
        float tri = (2.f / float(M_PI)) * asinf(std::clamp(s, -1.f, 1.f));
        float lfo = m_lfo(0.55f * s + 0.45f * tri);

        mPhase += 2.f * float(M_PI) * mRate / gam::sampleRate();
        if (mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        // --- Modulated delay with safety + smoothing ---
        float targetDelay = mBase + mDepth * lfo;
        targetDelay = std::clamp(targetDelay, 0.0001f, 0.045f);
        
        float d = targetDelay;
        mDelay.setDelay(d);

        // --- Process ---
        float wet = mDelay.process(x);

        // --- Final mix via DFXCore ---
        return mixOut(x, wet);
    }

    Modulator m_lfo;

private:
    Delay mDelay;
    
    float mPhase = 0.f;
    float mRate  = 0.3f;   // Hz
    float mDepth = 0.005f; // 5 ms
    float mBase  = 0.015f; // 15 ms
};
