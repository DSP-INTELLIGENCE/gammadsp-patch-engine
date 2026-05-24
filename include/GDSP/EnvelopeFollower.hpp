#pragma once
#include <algorithm>
#include <vector>
#include <cmath>


class EnvelopeFollower {
public:
    EnvelopeFollower() = default;

    void setAttack(float ms)  { mAttack  = coeffFromMs(ms); }
    void setRelease(float ms) { mRelease = coeffFromMs(ms); }

    float process(float x)
    {
        float level = std::abs(x);
        float c = (level > mEnv) ? mAttack : mRelease;
        mEnv += c * (level - mEnv);

        if (std::abs(mEnv) < (float)1e-20) mEnv = 0;
        return mEnv;
    }

    float value() const { return mEnv; }

private:
    static float coeffFromMs(float ms)
    {
        return (float)1 - std::exp((float)-1 / ((float)0.001 * ms * (float)gam::sampleRate()));
    }

    float mEnv = 0;
    float mAttack  = (float)0.01;
    float mRelease = (float)0.001;
};
