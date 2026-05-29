#pragma once
#include "DFXBBDDelay.hpp"

class DFXBBDFlanger : public Function
{
public:
    DFXBBDFlanger()
    {
        setRate(0.35f);
        setDepth(0.6f);
        setDelay(0.002f);   // 2 ms
        setMix(0.5f);
        setFeedback(0.6f);

        mDly.setBBD(1.3f,0.0018f,7000.0f,12000.0f,1.0f);
        mDly.setClockBleed(0.00045f);
    }

    void setRate(float n)    { mRate = clamp01(n); mDly.setLFO(expMap(mRate, 0.05f, 1.5f)); }
    void setDepth(float n)   { mDepth = clamp01(n); mDly.setDepth(0.0003f + mDepth * 0.0012f); }
    void setDelay(float sec) { mDly.setDelay(std::clamp(sec, 0.0003f, 0.008f)); }
    void setMix(float n)     { mMix = clamp01(n); mDly.setWet(mMix); mDly.setDry(1.0f - mMix); }
    void setFeedback(float f){ mDly.setFeedback(std::clamp(f, -0.95f, 0.95f)); }

    float process(float x) override { return mDly.process(x); }

private:
    DFXBBDDelay mDly;
    float mRate=0.35f, mDepth=0.6f, mMix=0.5f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }
    static float expMap(float n, float a, float b){
        return a + (b - a) * (std::exp(2.0f * n) - 1.f) / (std::exp(2.0f) - 1.f);
    }
};
