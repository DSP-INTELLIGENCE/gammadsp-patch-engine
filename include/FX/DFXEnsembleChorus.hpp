#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"

class DFXEnsembleChorus : public DFXCore
{
public:
    DFXEnsembleChorus()
    : d1(0.05f, 0.020f),
      d2(0.05f, 0.022f),
      d3(0.05f, 0.024f)
    {
        d1.setIpolType(gam::ipl::Type::CUBIC);
        d2.setIpolType(gam::ipl::Type::CUBIC);
        d3.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(0.45f);
    }

    void setRate(float hz)      { mRate = std::max(0.f, hz); }
    void setDepth(float ms)     { mDepth = std::max(0.f, ms * 0.001f); }
    void setBaseDelay(float ms) { mBase  = std::max(0.0005f, ms * 0.001f); }
    void setWidth(float w)      { mWidth = std::clamp(w, 0.f, 1.f); }

    float process(float x) override
    {
        float sr = gam::sampleRate();

        // 3-phase LFO network (analog ensemble behavior)
        float a = sinf(mPhase);
        float b = sinf(mPhase + 2.f * float(M_PI) / 3.f);
        float c = sinf(mPhase + 4.f * float(M_PI) / 3.f);

        // rounded triangle blend (BBD-style modulation)
        auto shape = [](float s)
        {
            float tri = (2.f / float(M_PI)) * asinf(std::clamp(s, -1.f, 1.f));
            return 0.55f * s + 0.45f * tri;
        };

        float l1 = shape(a);
        float l2 = shape(b);
        float l3 = shape(c);

        mPhase += 2.f * float(M_PI) * mRate / sr;
        if (mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        // Modulated delays — continuous motion, no stepping
        float t1 = std::clamp(mBase + mDepth * l1, 0.0001f, 0.045f);
        float t2 = std::clamp(mBase + mDepth * l2, 0.0001f, 0.045f);
        float t3 = std::clamp(mBase + mDepth * l3, 0.0001f, 0.045f);

        d1.setDelay(t1);
        d2.setDelay(t2);
        d3.setDelay(t3);

        float w1 = d1.process(x);
        float w2 = d2.process(x);
        float w3 = d3.process(x);

        // Ensemble stereo projection
        float L = w1 + 0.5f * w2 - 0.5f * w3;
        float R = w1 - 0.5f * w2 + 0.5f * w3;

        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R) * mWidth;
        float wet  = mid + side;

        return mixOut(x, wet);
    }

private:
    Delay d1, d2, d3;

    float mPhase = 0.f;
    float mRate  = 0.3f;
    float mDepth = 0.004f;
    float mBase  = 0.020f;
    float mWidth = 0.9f;
};
