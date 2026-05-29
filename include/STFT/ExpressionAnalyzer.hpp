#pragma once
#include "EnergyDynamics.hpp"
#include "OnsetTrigger.hpp"
#include "PitchTracker.hpp"
#include "NoisinessMeter.hpp"
#include "BeatTracker.hpp"

class ExpressionAnalyzer {
public:
    ExpressionAnalyzer(float sampleRate);

    void reset();

    void process(float x);

    float intensity() const { return mIntensity; }
    float articulation() const { return mArticulation; }
    float stability() const { return mStability; }
    float brightness() const { return mBrightness; }
    float expressiveness() const { return mExpressiveness; }

private:
    float mIntensity;
    float mArticulation;
    float mStability;
    float mBrightness;
    float mExpressiveness;

    EnergyDynamics mEnergy;
    OnsetTrigger   mOnset;
    PitchTracker   mPitch;
    NoisinessMeter mNoise;
    BeatTracker    mBeat;

    float mPrevPitch;
    float mPitchVariance;
};
