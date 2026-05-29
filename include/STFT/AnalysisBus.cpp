#include "AnalysisBus.hpp"

AnalysisBus::AnalysisBus(float sr)
: mActivity(sr),
  mOnset(sr),
  mEnergy(sr),
  mPitch(sr),
  mNoise(sr),
  mBeat(sr),
  mSection(sr),
  mExpressionAnalyzer(sr)
{}

void AnalysisBus::reset()
{
    mActivity.reset();
    mOnset.reset();
    mEnergy.reset();
    mPitch.reset();
    mNoise.reset();
    mBeat.reset();
    mSection.reset();
    mExpressionAnalyzer.reset();
}

float AnalysisBus::process(float x)
{
    // ---- Core feature extraction ----
    mActivity.processSample(x);
    float onsetCtrl = mOnset.process(x);
    float energy = mEnergy.process(x);
    float pitch  = mPitch.process(x);
    float noise  = mNoise.process(x);

    bool beat = mBeat.process(x);
    bool newSection = mSection.process(x);

    mExpressionAnalyzer.process(x);

    // ---- Fill feature structures ----
    mFeatures.active        = mActivity.isActive();
    mFeatures.onset         = mOnset.triggered();
    mFeatures.onsetStrength = mOnset.strength();
    mFeatures.energy        = energy;
    mFeatures.pitch         = pitch;
    mFeatures.noise         = noise;

    mMusical.tempo           = mBeat.tempo();
    mMusical.phase           = mBeat.phase();
    mMusical.section         = mSection.section();
    mMusical.sectionConfidence = mSection.confidence();

    mExpression.intensity     = mExpressionAnalyzer.intensity();
    mExpression.articulation  = mExpressionAnalyzer.articulation();
    mExpression.stability     = mExpressionAnalyzer.stability();
    mExpression.brightness    = mExpressionAnalyzer.brightness();
    mExpression.expressiveness= mExpressionAnalyzer.expressiveness();

    // AnalysisBus is passive — returns audio unchanged
    return x;
}
