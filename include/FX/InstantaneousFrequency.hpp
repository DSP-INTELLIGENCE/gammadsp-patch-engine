#pragma once
#include <cmath>
#include <algorithm>
#include "HilbertTransform.hpp"
#include "EnvFollow.hpp"


class InstantaneousFrequency {
public:
    InstantaneousFrequency()
    : mPrevPhase((float)0),
      mPhase((float)0),
      mFreq((float)0),
      mFreqEnv((float)0),
      mAmp((float)0),
      mFreqSmooth((float)10) // 10 Hz smoothing on frequency envelope
    {
        reset();
    }

    void reset()
    {
        mHilbert.reset();
        mPrevPhase = (float)0;
        mPhase     = (float)0;
        mFreq      = (float)0;
        mFreqEnv   = (float)0;
        mAmp       = (float)0;
    }

    // per-sample
    void process(float x)
    {
        auto iq = mHilbert.process(x);

        // Amplitude (analytic magnitude)
        mAmp = std::sqrt(iq.i * iq.i + iq.q * iq.q);

        // Phase
        mPhase = std::atan2(iq.q, iq.i);

        // Phase unwrap
        float d = mPhase - mPrevPhase;
        if (d >  (float)M_PI) d -= (float)2 * (float)M_PI;
        if (d < -(float)M_PI) d += (float)2 * (float)M_PI;
        mPrevPhase = mPhase;

        // Instantaneous frequency (Hz)
        mFreq = ((float)gam::sampleRate() / ((float)2 * (float)M_PI)) * d;

        // Smoothed frequency envelope
        mFreqEnv = mFreqSmooth.process(std::abs(mFreq));
    }

    float amplitude() const { return mAmp; }
    float phase()     const { return mPhase; }
    float frequency() const { return mFreq; }
    float freqEnv()   const { return mFreqEnv; }

private:
    HilbertTransform<float> mHilbert;

    float mPrevPhase;
    float mPhase;
    float mFreq;
    float mFreqEnv;
    float mAmp;

    EnvFollow<float> mFreqSmooth;
};
