#pragma once
#include <cmath>

class GrooveAnalyzer {
public:
    GrooveAnalyzer(float sampleRate);

    void reset();

    // feed onset timing and current beat phase
    void process(bool onset, float phase);

    float swing() const { return mSwing; }        // 0..1
    float tightness() const { return mTightness; } // 0..1
    float drift() const { return mDrift; }        // timing drift
    float stability() const { return mStability; } // groove confidence

private:
    float mSampleRate;

    float mMeanError;
    float mVariance;

    float mSwing;
    float mTightness;
    float mDrift;
    float mStability;
};
