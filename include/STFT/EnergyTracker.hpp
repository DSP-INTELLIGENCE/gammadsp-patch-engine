#pragma once
#include <algorithm>
#include <cmath>

#include "EnvFollow.hpp"
#include "RMSFollower.hpp"


class EnergyTracker {
public:
    EnergyTracker()
    : mEnv((float)40),       // 40 Hz envelope speed
      mRMS((float)0.05),     // 50 ms RMS window
      mEnergy((float)0)
    {}

    void setEnvSpeed(float hz)
    {
        mEnv.setFreq(std::max((float)1, hz));
    }

    void setRmsWindow(float seconds)
    {
        mRMS.setWindow(seconds);
    }

    void reset()
    {
        mEnergy = (float)0;
    }

    // per-sample
    float process(float x)
    {
        float e = mEnv.process(std::abs(x));
        float r = mRMS.process(x);

        // Psychoacoustic blend: transient + body
        float raw = (float)0.6 * e + (float)0.4 * r;

        // Final energy smoothing (very slow, perceptual)
        mEnergy = (float)0.995 * mEnergy + (float)0.005 * raw;

        return mEnergy;
    }

    // Continuous 0..1 intensity signal
    float energy() const { return mEnergy; }

private:
    EnvFollow<float>   mEnv;
    RMSFollower<float> mRMS;

    float mEnergy;
};
