// LookaheadLimiter.hpp
#pragma once
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "EnvFollow.hpp"

class LookaheadLimiter {
public:
    LookaheadLimiter(float sampleRate, float lookaheadMs = 5.0f);

    void setCeiling(float c);
    void setAttack(float sec);
    void setRelease(float sec);
    void setLookahead(float ms);
    void setMakeup(float g);

    void reset();

    float process(float x);

private:
    float computeCoeff(float time) const;

    float mSampleRate;
    unsigned mDelaySize;
    unsigned mDelayIndex;

    float* mDelayBuffer;

    EnvFollow mEnv;

    float mCeiling;
    float mAttack;
    float mRelease;
    float mMakeup;

    float mGain;
};
