#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"


class DFXStereoChorus : public DFXCore
{
public:
    DFXStereoChorus()
    : mDelayL(0.05f, 0.02f),
      mDelayR(0.05f, 0.021f)
    {
        mDelayL.setIpolType(gam::ipl::Type::CUBIC);
        mDelayR.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(0.4f);
    }

    void setRate(float hz)      { mRate = std::max(0.f, hz); }
    void setDepth(float ms)     { mDepth = std::max(0.f, ms * 0.001f); }
    void setBaseDelay(float ms) { mBase  = std::max(0.0005f, ms * 0.001f); }
    void setWidth(float w)      { mWidth = std::clamp(w, 0.f, 1.f); }

    // ---------- Public Interfaces ----------

    float process(float x) override
    {
        StereoSample s = processInternal(x);
        return 0.5f * (s.outL + s.outR); // mono fold-down
    }

    StereoSample processStereo(float x)
    {
        return processInternal(x);
    }

private:
    // ---------- Single Internal DSP Step ----------

    StereoSample processInternal(float x)
    {
        float sr = gam::sampleRate();

        // LFOs
        float sL = sinf(mPhase);
        float sR = sinf(mPhase + float(M_PI_2));

        auto shape = [](float s)
        {
            float tri = (2.f / float(M_PI)) * asinf(std::clamp(s, -1.f, 1.f));
            return 0.55f * s + 0.45f * tri;
        };

        float lfoL = shape(sL);
        float lfoR = shape(sR);

        float rate  = m_rate.process(mRate);
        float depth = m_depth.process(mDepth);
        float width = m_width.process(mWidth);

        mPhase += 2.f * float(M_PI) * rate / sr;
        if (mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        float tL = std::clamp(mBase + depth * lfoL, 0.0001f, 0.045f);
        float tR = std::clamp(mBase + depth * lfoR, 0.0001f, 0.045f);

        mDelayL.setDelay(tL);
        mDelayR.setDelay(tR);

        float wetL = mDelayL.process(x);
        float wetR = mDelayR.process(x);

        // Width matrix
        float mid  = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR) * width;

        float L = mixOut(x, mid + side);
        float R = mixOut(x, mid - side);

        return { L, R };
    }

    // ---------- State ----------

    Delay mDelayL, mDelayR;

    Modulator m_rate, m_depth, m_width;

    float mPhase = 0.f;
    float mRate  = 0.3f;
    float mDepth = 0.005f;
    float mBase  = 0.015f;
    float mWidth = 0.85f;
};
