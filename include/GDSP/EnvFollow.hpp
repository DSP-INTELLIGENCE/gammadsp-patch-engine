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


class EnvFollow : public Function {
public:
    
    EnvFollow(float freq=10.0f)
    : 
    mFreq(freq),
    mEnv(freq)
    {
    }

    void setFreq(float freqHz)
    {
        mFreq = std::max(0.01f, freqHz);
        mEnv.lpf.freq(mFreq);
    }

    void setLag(float seconds)
    {
        float f = (seconds > 0.0f) ? (1.0f / seconds) : 1000.0f;
        setFreq(f);
    }

    void reset()
    {
        mEnv.lpf.reset(0.0f);
    }

    float process(float input) override
    {
        return mEnv(input);
    }

    float value() const
    {
        return mEnv.value();
    }

    bool done(float eps=0.001f) const
    {
        return mEnv.done(eps);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float mFreq = 10.0f;
    float mLag  = 0.0;

private:
    gam::EnvFollow<double> mEnv;
    
};




