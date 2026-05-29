#pragma once
#include <algorithm>


class EnvFollow {
public:
    EnvFollow(float attackHz = 20.0f, float releaseHz = 5.0f)
    : mValue(0.0f)
    {
        setAttack(attackHz);
        setRelease(releaseHz);
    }

    void setAttack(float freqHz)  { mAttackCoeff  = calcCoeff(freqHz); }
    void setRelease(float freqHz) { mReleaseCoeff = calcCoeff(freqHz); }

    void setLag(float seconds)
    {
        if(seconds <= 0) setAttack(1000.0f), setRelease(1000.0f);
        else setAttack(1.0f/seconds), setRelease(1.0f/seconds);
    }

    void reset(float v = 0) { mValue = v; }

    float process(float x)
    {
        float input = std::abs(x);
        float coeff = (input > mValue) ? mAttackCoeff : mReleaseCoeff;
        mValue = (1 - coeff) * input + coeff * mValue;
        return mValue;
    }

    float value() const { return mValue; }

private:
    float calcCoeff(float freq)
    {
        freq = std::max((float)0.01, freq);
        return std::exp(-2 * 3.14159265 * freq / gam::sampleRate());
    }

    float mAttackCoeff, mReleaseCoeff;
    float mValue;
};
