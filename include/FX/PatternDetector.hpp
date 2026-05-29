#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class PatternDetector {
public:
    struct PatternState {
        Sample repetition = (Sample)0;   // 0..1
        Sample predictability = (Sample)0; // 0..1
        bool   repeating = false;
    };

    PatternDetector()
    {
        reset();
    }

    void reset()
    {
        mLastInterval = 0;
        mMeanInterval = 0;
        mVariance     = 0;
        mConfidence   = (Sample)0;
    }

    // Call on each detected onset
    void processOnset()
    {
        unsigned now = mClock++;
        if (mLastOnset != 0)
        {
            unsigned interval = now - mLastOnset;
            analyzeInterval(interval);
        }
        mLastOnset = now;
    }

    // Call every sample
    PatternState process()
    {
        // Slowly decay confidence if nothing happens
        mConfidence *= (Sample)0.9995;

        PatternState s;
        s.repetition   = mConfidence;
        s.predictability = (Sample)1 - std::min(mVariance * (Sample)2, (Sample)1);
        s.repeating = (mConfidence > (Sample)0.6);
        return s;
    }

private:
    void analyzeInterval(unsigned interval)
    {
        Sample delta = std::abs((Sample)interval - mMeanInterval);

        mMeanInterval = (Sample)0.9 * mMeanInterval + (Sample)0.1 * interval;
        mVariance     = (Sample)0.9 * mVariance + (Sample)0.1 * delta;

        Sample stability = (Sample)1 - std::min(delta / std::max((Sample)1, mMeanInterval), (Sample)1);
        mConfidence = (Sample)0.8 * mConfidence + (Sample)0.2 * stability;
    }

private:
    unsigned mClock = 0;
    unsigned mLastOnset = 0;

    Sample mMeanInterval = (Sample)0;
    Sample mVariance = (Sample)0;
    Sample mConfidence = (Sample)0;
};
