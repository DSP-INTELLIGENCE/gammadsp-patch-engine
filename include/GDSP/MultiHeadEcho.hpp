#pragma once
#include "ModMultitap.hpp"
#include "OnePole.hpp"
#include "SoftClipper.hpp"

class MultiHeadEcho : public Function
{
public:
    MultiHeadEcho(unsigned heads = 4, float maxDelay = 2.0f)
    : mEcho(heads, maxDelay),
      mToneLPF(12000.0f, gam::LOW_PASS)
    {
        setPattern(0);
        setTime(0.42f);
        setFeedback(0.45f);
        setMix(0.35f);
        setColor(0.5f);
        setDrive(0.5f);
    }

    // --- Musical Controls ---

    void setTime(float sec)
    {
        mBaseTime = std::clamp(sec, 0.15f, 1.2f);
        applyPattern();
    }

    void setFeedback(float f)
    {
        mFeedback = std::clamp(f, 0.0f, 0.85f);
        updateFeedback();
    }

    void setMix(float m)
    {
        mEcho.setGlobalMix(std::clamp(m, 0.f, 1.f));
    }

    void setColor(float c)
    {
        mColor = std::clamp(c, 0.f, 1.f);
    }

    void setDrive(float d)
    {
        mDrive = std::clamp(d, 0.f, 2.f);
    }

    // Patterns like classic echo machines
    void setPattern(int p)
    {
        mPattern = p % 4;
        applyPattern();
    }

    float process(float input) override
    {
        float x = input;

        // pre color
        mToneLPF.setFreq(14000.f - mColor * 11000.f);
        x = mToneLPF.process(x);

        // multi-head network
        float y = mEcho.process(x);

        // feedback color + saturation
        y = mClip.process(y);

        return y;
    }

private:
    void updateFeedback()
    {
        //for(unsigned i = 0; i < mEcho.getNumTaps(); ++i)
            mEcho.setFBM(0, mFeedback);
    }

    void applyPattern()
    {
        static const float patterns[4][4] =
        {
            {1.0f, 0.0f, 0.0f, 0.0f},   // single head
            {1.0f, 0.5f, 0.0f, 0.0f},   // dual
            {1.0f, 0.66f, 0.33f, 0.0f}, // triple
            {1.0f, 0.75f, 0.5f, 0.25f}  // quad
        };

        for(unsigned i = 0; i < mEcho.getNumTaps(); ++i)
        {
            float ratio = patterns[mPattern][i];
            if(ratio <= 0.0f)
                mEcho.setMIXM(i, 0.0f);
            else
            {
                mEcho.setTapDelay(i, mBaseTime * ratio);
                mEcho.setMIXM(i, 1.0f - 0.15f * i);
            }
        }

        updateFeedback();
    }

private:
    ModMultitap mEcho;
    OnePole     mToneLPF;
    SoftClipper mClip;

    float mBaseTime = 0.42f;
    float mFeedback = 0.45f;
    float mColor    = 0.5f;
    float mDrive    = 0.5f;
    int   mPattern  = 0;
};
