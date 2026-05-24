#pragma once
#include <algorithm>
#include <cstdint>

template <class Sample>
class RandomMotion {
public:
    RandomMotion()
    {
        reset();
    }

    void reset(uint32_t seed = 1)
    {
        mState = seed;
        mValue = (Sample)0;
        mTarget = (Sample)0;
    }

    // How fast the randomness moves (Hz)
    void setRate(Sample hz)
    {
        mRate = std::max((Sample)0.001, hz);
    }

    // How wild the motion is (0..1)
    void setAmount(Sample a)
    {
        mAmount = std::clamp(a, (Sample)0, (Sample)1);
    }

    // Per-sample
    Sample process()
    {
        // Update target at random intervals
        mPhase += mRate / (Sample)gam::sampleRate();

        if (mPhase >= (Sample)1)
        {
            mPhase -= (Sample)1;
            mTarget = (Sample)rand01() * (Sample)2 - (Sample)1;
        }

        // Smooth toward target
        mValue += (Sample)0.01 * (mTarget - mValue);

        return mValue * mAmount;
    }

    Sample value() const { return mValue * mAmount; }

private:
    uint32_t mState = 1;
    Sample   mPhase = (Sample)0;
    Sample   mValue = (Sample)0;
    Sample   mTarget = (Sample)0;
    Sample   mRate = (Sample)1;
    Sample   mAmount = (Sample)0.5;

    uint32_t rand01()
    {
        mState = mState * 1664525u + 1013904223u;
        return (mState >> 16) & 0x7FFF;
    }
};
