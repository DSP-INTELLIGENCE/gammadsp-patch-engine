// StudioCompressor.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include "EnvFollow.hpp"
#include "RMSFollower.hpp"

template<class Sample>
class StudioCompressor {
public:
    StudioCompressor(Sample sampleRate);

    void setThreshold(Sample t);   // linear 0..1
    void setRatio(Sample r);       // >= 1
    void setAttack(Sample sec);
    void setRelease(Sample sec);
    void setKnee(Sample k);        // 0..1
    void setMakeup(Sample g);

    Sample process(Sample x);

private:
    Sample computeGain(Sample level);

    EnvFollow   mEnv;
    RMSFollower mRMS;

    Sample mThreshold;
    Sample mRatio;
    Sample mAttack;
    Sample mRelease;
    Sample mKnee;
    Sample mMakeup;

    Sample mGain;
    Sample mSampleRate;
};

template<class Sample>
StudioCompressor<Sample>::StudioCompressor()
: mEnv(30.0f),
  mRMS(0.05f),
  mThreshold(0.2f),
  mRatio(4.0f),
  mAttack(0.005f),
  mRelease(0.08f),
  mKnee(0.2f),
  mMakeup(1.0f),
  mGain(1.0f),

{}

template<class Sample> void StudioCompressor<Sample>::setThreshold(Sample t){ mThreshold = std::max(0.0001f, t); }
template<class Sample> void StudioCompressor<Sample>::setRatio(Sample r){ mRatio = std::max(1.0f, r); }
template<class Sample> void StudioCompressor<Sample>::setAttack(Sample s){ mAttack = std::max(0.0001f, s); }
template<class Sample> void StudioCompressor<Sample>::setRelease(Sample s){ mRelease = std::max(0.0001f, s); }
template<class Sample> void StudioCompressor<Sample>::setKnee(Sample k){ mKnee = std::clamp(k, 0.0f, 1.0f); }
template<class Sample> void StudioCompressor<Sample>::setMakeup(Sample g){ mMakeup = g; }

template<class Sample>
Sample StudioCompressor<Sample>::computeGain(Sample level)
{
    // prevent divide-by-zero & instability
    level = std::max(level, 1e-8f);

    // classic compressor curve
    Sample compressed = mThreshold + (level - mThreshold) / mRatio;

    // hard knee boundaries
    if(level <= mThreshold - mKnee)
        return 1.0f;

    if(level >= mThreshold + mKnee)
        return compressed / level;

    // soft knee region
    Sample x = (level - (mThreshold - mKnee)) / (2.0f * mKnee);
    x = std::clamp(x, 0.0f, 1.0f);

    // smoothstep curve
    Sample soft = x * x * (3.0f - 2.0f * x);

    // interpolate between dry & compressed
    Sample y = level + soft * (compressed - level);

    return y / level;
}

template<class Sample>
Sample StudioCompressor<Sample>::process(Sample x)
{
    // detector
    Sample env = mEnv.process(std::abs(x));
    Sample rms = mRMS.process(x);

    Sample level = 0.7f * env + 0.3f * rms;

    // compute target gain
    Sample target = computeGain(level);

    // choose correct time constant
    Sample time = (target < mGain) ? mAttack : mRelease;
    Sample coeff = std::exp(-1.0f / (time * mSampleRate));

    // smooth gain change
    mGain = target + coeff * (mGain - target);

    // apply gain & makeup
    return x * mGain * mMakeup;
}
