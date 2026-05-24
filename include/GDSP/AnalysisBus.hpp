#pragma once
#include "ActivityAnalyzer.hpp"
#include "OnsetTrigger.hpp"
#include "EnergyDynamics.hpp"
#include "PitchTracker.hpp"
#include "NoisinessMeter.hpp"
#include "BeatTracker.hpp"
#include "SectionDetector.hpp"
#include "ExpressionAnalyzer.hpp"

struct AudioFeatures {
    bool  active;
    bool  onset;
    float onsetStrength;
    float energy;
    float pitch;
    float noise;
};

struct MusicalState {
    float tempo;
    float phase;
    int   section;
    float sectionConfidence;
};

struct ExpressionState {
    float intensity;
    float articulation;
    float stability;
    float brightness;
    float expressiveness;
};

class AnalysisBus {
public:
    AnalysisBus(float sampleRate);

    void reset();

    // one call per sample
    float process(float x);

    const AudioFeatures&   features()   const { return mFeatures; }
    const MusicalState&   musical()    const { return mMusical; }
    const ExpressionState& expression() const { return mExpression; }

private:
    ActivityAnalyzer  mActivity;
    OnsetTrigger      mOnset;
    EnergyDynamics    mEnergy;
    PitchTracker      mPitch;
    NoisinessMeter    mNoise;
    BeatTracker       mBeat;
    SectionDetector   mSection;
    ExpressionAnalyzer mExpressionAnalyzer;

    AudioFeatures    mFeatures;
    MusicalState     mMusical;
    ExpressionState  mExpression;
};
