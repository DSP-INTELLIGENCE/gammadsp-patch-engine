#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class PhaseLockedLoop {
public:
    PhaseLockedLoop()
    {
        reset();
    }

    void reset()
    {
        mFreq       = (Sample)0;
        mPhase      = (Sample)0;
        mConfidence = (Sample)0;
        mLocked     = false;
    }

    // Drive with measured frequency in Hz
    void process(Sample inputFreq)
    {
        Sample error = inputFreq - mFreq;

        if (std::abs(error) < mLockRange)
        {
            mFreq += error * mResp;
            mConfidence += (Sample)0.01;
        }
        else
        {
            mConfidence -= (Sample)0.02;
        }

        mConfidence = std::clamp(mConfidence, (Sample)0, (Sample)1);
        mLocked = (mConfidence > (Sample)0.6);

        // Integrate phase using global engine timebase
        mPhase += ((Sample)2 * (Sample)M_PI * mFreq) / (Sample)gam::sampleRate();
        if (mPhase > (Sample)2 * (Sample)M_PI)
            mPhase -= (Sample)2 * (Sample)M_PI;
    }

    Sample frequency()  const { return mFreq; }
    Sample phase()      const { return mPhase; }
    bool   locked()     const { return mLocked; }
    Sample confidence() const { return mConfidence; }

    // Loop parameters
    void setResponsiveness(Sample r)
    {
        mResp = std::clamp(r, (Sample)0.001, (Sample)1);
    }

    void setLockRange(Sample hz)
    {
        mLockRange = std::max((Sample)0.1, hz);
    }

private:
    Sample mFreq       = (Sample)0;
    Sample mPhase      = (Sample)0;

    Sample mResp       = (Sample)0.05;
    Sample mLockRange  = (Sample)10;

    Sample mConfidence = (Sample)0;
    bool   mLocked     = false;
};
