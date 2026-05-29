#pragma once
#include "DFXBBDDelay.hpp"

class DFXBBDChorus : public Function
{
public:
    DFXBBDChorus()
    {
        // Classic chorus defaults
        setRate(0.35f);
        setDepth(0.45f);
        setDelay(0.020f);     // 20 ms
        setMix(0.55f);
        setFeedback(0.0f);

        // BBD voicing
        mDly.setBBD(1.1f,0.0012f,7000.0f, 12000.0f,1.0f);
        mDly.setClockBleed(0.00025f);
    }

    void setRate(float n)    { mRate = clamp01(n); mDly.setLFO(expMap(mRate, 0.05f, 3.0f)); }
    void setDepth(float n)   { mDepth = clamp01(n); mDly.setDepth(0.0015f + mDepth * 0.004f); }
    void setDelay(float sec) { mDly.setDelay(sec); }
    void setMix(float n)     { mMix = clamp01(n); mDly.setWet(mMix); mDly.setDry(1.0f - mMix); }
    void setFeedback(float f){ mDly.setFeedback(std::clamp(f, 0.f, 0.3f)); }

    float process(float x) override { return mDly.process(x); }

private:
    DFXBBDDelay mDly;
    float mRate=0.35f, mDepth=0.45f, mMix=0.55f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }
    static float expMap(float n, float a, float b){
        return a + (b - a) * (std::exp(2.5f * n) - 1.f) / (std::exp(2.5f) - 1.f);
    }
};
