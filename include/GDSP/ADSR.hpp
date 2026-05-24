#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"

class ADSR : public Generator {
public:
    ADSR(float attack  = 0.01f,
         float decay   = 0.1f,
         float sustain = 0.7f,
         float release = 0.3f,
         float level   = 1.0f,
         float curve   = -4.0f)
    : mAttack(attack),
    mDecay(decay),
    mSustain(sustain),
    mRelease(release),
    mLevel(level),
    mCurve(curve)
    {
        mEnv.attack(mAttack);
        mEnv.decay(mDecay);
        mEnv.sustain(mSustain);
        mEnv.release(mRelease);
        mEnv.amp(mLevel);
        mEnv.curve(mCurve);

        mEnv.reset();
    }

    void setAttack(float seconds)
    {
        mAttack = std::max(0.f, seconds);
        mEnv.attack(mAttack);
    }

    void setDecay(float seconds)
    {
        mDecay = std::max(0.f, seconds);
        mEnv.decay(mDecay);
    }

    void setSustain(float level)
    {
        mSustain = std::clamp(level, 0.f, 1.f);
        mEnv.sustain(mSustain);
    }

    void setRelease(float seconds)
    {
        mRelease = std::max(0.f, seconds);
        mEnv.release(mRelease);
    }

    void setLevel(float level)
    {
        mLevel = std::max(0.f, level);
        mEnv.amp(mLevel);
    }

    void setCurve(float curve)
    {
        mCurve = curve;
        mEnv.curve(mCurve);
    }

    void noteOn()
    {
        mEnv.reset();
    }

    void noteOff()
    {
        mEnv.release();
    }

    void reset()
    {
        mEnv.reset();
    }

    bool done() const
    {
        return mEnv.done();
    }

    float process() override
    {
        return mEnv();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }


private:
    gam::ADSR<double, double> mEnv;

    float mAttack;
    float mDecay;
    float mSustain;
    float mRelease;
    float mLevel;
    float mCurve;
};

