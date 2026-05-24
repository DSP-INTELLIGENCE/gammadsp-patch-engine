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

class AD : public Generator {
public:
    AD(float attack = 0.01f,
       float decay  = 0.1f,
       float level  = 1.0f,
       float curve  = -4.0f)
    : mAttack(attack),
    mDecay(decay),
    mLevel(level),
    mCurve(curve)
    {        
        mEnv.attack(mAttack);
        mEnv.decay(mDecay);
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

    void trigger()
    {
        mEnv.reset();
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
    gam::AD<double, double> mEnv;

    float mAttack;
    float mDecay;
    float mLevel;
    float mCurve;
};


