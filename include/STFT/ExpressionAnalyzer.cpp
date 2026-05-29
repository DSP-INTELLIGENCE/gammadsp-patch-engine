#include "ExpressionAnalyzer.hpp"
#include <cmath>
#include <algorithm>

ExpressionAnalyzer::ExpressionAnalyzer(float sr)
: mIntensity(0.0f),
  mArticulation(0.0f),
  mStability(0.0f),
  mBrightness(0.0f),
  mExpressiveness(0.0f),
  mEnergy(sr),
  mOnset(sr),
  mPitch(sr),
  mNoise(sr),
  mBeat(sr),
  mPrevPitch(0.0f),
  mPitchVariance(0.0f)
{}

void ExpressionAnalyzer::reset()
{
    mIntensity = mArticulation = mStability = mBrightness = mExpressiveness = 0.0f;
    mPrevPitch = 0.0f;
    mPitchVariance = 0.0f;

    mEnergy.reset();
    mOnset.reset();
    mPitch.reset();
    mNoise.reset();
    mBeat.reset();
}

void ExpressionAnalyzer::process(float x)
{
    float e = mEnergy.process(x);
    bool  o = mOnset.process(x) > 0.0f;
    float p = mPitch.process(x);
    float n = mNoise.process(x);
    mBeat.process(x);

    // Intensity: smoothed energy
    mIntensity = 0.98f * mIntensity + 0.02f * e;

    // Articulation: onset frequency
    float a = o ? 1.0f : 0.0f;
    mArticulation = 0.95f * mArticulation + 0.05f * a;

    // Stability: inverse of pitch variance
    if(p > 0.0f && mPrevPitch > 0.0f)
    {
        float diff = std::abs(p - mPrevPitch);
        mPitchVariance = 0.99f * mPitchVariance + 0.01f * diff;
    }
    mPrevPitch = p;

    mStability = 1.0f / (1.0f + mPitchVariance * 10.0f);

    // Brightness: noisiness + transients
    mBrightness = 0.7f * mBrightness + 0.3f * n;

    // Expressiveness: combined score
    mExpressiveness =
        0.30f * mIntensity +
        0.25f * mArticulation +
        0.20f * mBrightness +
        0.25f * mStability;
}
