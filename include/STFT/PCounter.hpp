#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class PCounter {
public:
    PCounter() { reset(); }

    void reset()
    {
        mSamples     = 0;
        mPeriod      = 0;
        mStableCount = 0;
        mLocked      = false;
    }

    // Call once per sample. trigger=true when event occurs.
    bool process(bool trigger)
    {
        ++mSamples;

        if (trigger)
        {
            if (mPeriod == 0)
            {
                mPeriod = mSamples;
            }
            else
            {
                Sample err = std::abs((Sample)mSamples - (Sample)mPeriod) / (Sample)mPeriod;

                if (err < (Sample)0.12)
                {
                    mPeriod = (unsigned)((Sample)0.85 * mPeriod + (Sample)0.15 * mSamples);
                    ++mStableCount;
                }
                else
                {
                    mStableCount = 0;
                }
            }

            mLocked  = (mStableCount > 3);
            mSamples = 0;
            return true;
        }

        return false;
    }

    // Samples since last trigger
    unsigned samples() const { return mSamples; }

    // Last measured period (in samples)
    unsigned period() const { return mPeriod; }

    // Estimated frequency (Hz)
    Sample frequency() const
    {
        if (mPeriod == 0) return (Sample)0;
        return (Sample)gam::sampleRate() / (Sample)mPeriod;
    }

    // Estimated tempo (BPM)
    Sample tempo() const
    {
        return frequency() * (Sample)60;
    }

    bool locked() const { return mLocked; }

private:
    unsigned mSamples     = 0;
    unsigned mPeriod      = 0;
    unsigned mStableCount = 0;
    bool     mLocked      = false;
};
