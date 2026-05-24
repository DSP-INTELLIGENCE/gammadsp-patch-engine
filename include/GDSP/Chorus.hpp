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

class Chorus : public Function {
public:
    Chorus(float delay = 0.0021f,
           float depth = 0.002f,
           float freq  = 1.0f,
           float ffd   = 0.9f,
           float fbk   = 0.1f)
    :   
    mChorus(delay, depth, freq, ffd, fbk)
    {
    
    }

    void setDelay(float seconds)
    {
        float mDelay = std::max(0.f, seconds);
        mChorus.delay(mDelay);
    }

    void setDepth(float seconds)
    {
        float mDepth = std::max(0.f, seconds);
        mChorus.depth(mDepth);
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.f, hz);
        mChorus.freq(mFreq);
    }

    void setFeedforward(float v)
    {
        float mFFD = std::clamp(v, -1.f, 1.f);
        mChorus.ffd(mFFD);
    }

    void setFeedback(float v)
    {
        float mFBK = std::clamp(v, -0.999f, 0.999f);
        mChorus.fbk(mFBK);
    }

    void setMaxDelay(float seconds)
    {
        mChorus.maxDelay(std::max(0.f, seconds));
    }

    float process(float input) override
    {
        return mChorus(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Chorus<double> mChorus;   
};

