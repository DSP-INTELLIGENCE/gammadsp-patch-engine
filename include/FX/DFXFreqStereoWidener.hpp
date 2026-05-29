#pragma once
#include "Delay.hpp"
#include "OnePole.hpp"
#include "DFXCore.hpp"

class DFXFreqStereoWidener : public DFXCore
{
public:
    DFXFreqStereoWidener()
    : mDL(0.01f, 0.003f),
      mDR(0.01f, 0.004f),
      mLowCut(200.0f, gam::FilterType::LOW_PASS),
      mHighCut(4000.0f, gam::FilterType::HIGH_PASS)
    {
        mDL.setIpolType(gam::ipl::Type::CUBIC);
        mDR.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(1.0f);
    }

    // 0..1
    void setWidth(float w) { mWidth = std::clamp(w, 0.f, 1.f); }

    // Hz
    void setLowFreq(float hz)  { mLowCut.freq(std::max(20.f, hz)); }
    void setHighFreq(float hz) { mHighCut.freq(std::max(500.f, hz)); }

    float process(float x) override
    {
        // split bands
        float low  = mLowCut.process(x);
        float high = mHighCut.process(x);
        float mid  = x - low - high;

        // widen only mids + highs
        float tL = 0.0003f + mWidth * 0.0025f;
        float tR = 0.0006f + mWidth * 0.0035f;

        mDL.setDelay(tL);
        mDR.setDelay(tR);

        float wL = mDL.process(mid + high);
        float wR = mDR.process(mid + high);

        // keep lows mono
        float L = low + wL;
        float R = low + wR;

        float midMix  = 0.5f * (L + R);
        float sideMix = 0.5f * (L - R) * mWidth;

        float wet = midMix + sideMix;

        return mixOut(x, wet);
    }

private:
    Delay   mDL, mDR;
    OnePole mLowCut, mHighCut;

    float mWidth = 0.7f;
};
