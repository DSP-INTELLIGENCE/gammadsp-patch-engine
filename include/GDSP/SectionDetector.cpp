#include "SectionDetector.hpp"
#include <algorithm>
#include <cmath>

SectionDetector::SectionDetector(float sr)
: mSampleRate(sr),
  mEnergy(sr),
  mPitch(sr),
  mNoise(sr),
  mEnergyAvg(0), mPitchAvg(0), mNoiseAvg(0),
  mEnergyLong(0), mPitchLong(0), mNoiseLong(0),
  mConfidence(0),
  mSection(0),
  mRefractory((unsigned)(sr * 1.5f)),   // 1.5 sec lockout
  mRefractoryLeft(0)
{}

void SectionDetector::reset()
{
    mEnergy.reset();
    mPitch.reset();
    mNoise.reset();

    mEnergyAvg = mPitchAvg = mNoiseAvg = 0.0f;
    mEnergyLong = mPitchLong = mNoiseLong = 0.0f;

    mConfidence = 0.0f;
    mSection = 0;
    mRefractoryLeft = 0;
}

bool SectionDetector::process(float x)
{
    float e = mEnergy.process(x);
    float p = mPitch.process(x);
    float n = mNoise.process(x);

    // short-term memory
    mEnergyAvg = 0.99f   * mEnergyAvg + 0.01f   * e;
    mPitchAvg  = 0.995f  * mPitchAvg  + 0.005f  * p;
    mNoiseAvg  = 0.99f   * mNoiseAvg  + 0.01f   * n;

    // long-term memory
    mEnergyLong = 0.9995f * mEnergyLong + 0.0005f * e;
    mPitchLong  = 0.9995f * mPitchLong  + 0.0005f * p;
    mNoiseLong  = 0.9995f * mNoiseLong  + 0.0005f * n;

    float dE = std::abs(mEnergyAvg - mEnergyLong);
    float dP = std::abs(mPitchAvg  - mPitchLong);
    float dN = std::abs(mNoiseAvg  - mNoiseLong);

    float delta = dE * 1.0f + dP * 0.6f + dN * 0.4f;

    bool newSection = false;

    if(mRefractoryLeft > 0)
        --mRefractoryLeft;

    // context gating: ignore low-energy & noise-only transitions
    bool validContext = (mEnergyAvg > 0.05f && mNoiseAvg < 0.6f);

    if(delta > 0.18f && validContext && mRefractoryLeft == 0)
    {
        newSection = true;
        mSection++;
        mConfidence = std::min(1.0f, mConfidence + 0.25f);
        mRefractoryLeft = mRefractory;
    }
    else
    {
        mConfidence *= 0.9995f;
    }

    return newSection;
}
