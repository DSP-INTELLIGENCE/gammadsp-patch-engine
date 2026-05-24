#include "GrooveAnalyzer.hpp"
#include <algorithm>

GrooveAnalyzer::GrooveAnalyzer(float sr)
: mSampleRate(sr),
  mMeanError(0.0f),
  mVariance(0.0f),
  mSwing(0.0f),
  mTightness(0.0f),
  mDrift(0.0f),
  mStability(0.0f)
{}

void GrooveAnalyzer::reset()
{
    mMeanError = 0.0f;
    mVariance = 0.0f;
    mSwing = 0.0f;
    mTightness = 0.0f;
    mDrift = 0.0f;
    mStability = 0.0f;
}

void GrooveAnalyzer::process(bool onset, float phase)
{
    if(!onset) return;

    // error relative to nearest beat center
    float error = phase < 0.5f ? phase : phase - 1.0f;

    // integrate timing statistics
    mMeanError = 0.98f * mMeanError + 0.02f * error;
    mVariance  = 0.98f * mVariance  + 0.02f * std::abs(error);

    // swing: consistent late hits on offbeats
    float swingTarget = std::clamp(mMeanError * 2.0f + 0.5f, 0.0f, 1.0f);

    // tightness: low variance
    float tightTarget = 1.0f - std::min(mVariance * 6.0f, 1.0f);

    // drift: accumulated offset
    mDrift = mMeanError;

    // stability: how reliable the groove is
    float stabTarget = tightTarget;

    // smooth outputs
    mSwing    = 0.9f * mSwing    + 0.1f * swingTarget;
    mTightness= 0.9f * mTightness+ 0.1f * tightTarget;
    mStability= 0.95f * mStability + 0.05f * stabTarget;
}
