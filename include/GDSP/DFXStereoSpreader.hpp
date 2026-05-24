#pragma once
#include "Delay.hpp"
#include "DFXCore.hpp"

class DFXStereoSpreader : public DFXCore
{
public:
    DFXStereoSpreader()
    : mDL(0.01f, 0.003f),
      mDR(0.01f, 0.004f)
    {
        mDL.setIpolType(gam::ipl::Type::CUBIC);
        mDR.setIpolType(gam::ipl::Type::CUBIC);

        setWet(1.0f);
        setDry(1.0f);
        setMix(1.0f);
    }

    // 0 = mono, 1 = maximum width
    void setWidth(float w) { mWidth = std::clamp(w, 0.f, 1.f); }

    float process(float x) override
    {
        // micro decorrelation delays
        float tL = 0.0003f + mWidth * 0.0030f;
        float tR = 0.0005f + mWidth * 0.0040f;

        mDL.setDelay(tL);
        mDR.setDelay(tR);

        float l = mDL.process(x);
        float r = mDR.process(x);

        // mid / side shaping
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * mWidth;

        float wet = mid + side;

        return mixOut(x, wet);
    }

private:
    Delay mDL, mDR;
    float mWidth = 0.75f;
};
