#pragma once
#include "EnergyDynamics.hpp"
#include "PitchTracker.hpp"
#include "NoisinessMeter.hpp"

class SectionDetector {
public:
    SectionDetector(float sampleRate);

    void reset();

    // returns true when a new section is detected
    bool process(float x);

    int   section()   const { return mSection; }
    float confidence() const { return mConfidence; }

private:
    float mSampleRate;

    EnergyDynamics mEnergy;
    PitchTracker   mPitch;
    NoisinessMeter mNoise;

    float mEnergyAvg, mPitchAvg, mNoiseAvg;
    float mEnergyLong, mPitchLong, mNoiseLong;

    float mConfidence;
    int   mSection;

    unsigned mRefractory;
    unsigned mRefractoryLeft;
};

