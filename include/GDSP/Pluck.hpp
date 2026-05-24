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

class Pluck : public Generator, public Function {
public:
    Pluck(float freq  = 440.f,
          float decay = 0.99f)
    :   
    mPluck(freq, decay)
    {
    
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(1.f, hz);
        mPluck.freq(mFreq);
    }

    void reset()
    {
        mPluck.reset();
    }

    float process() override
    {
        return mPluck();
    }

    float process(float input) override
    {
        return mPluck(input);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }


private:
    gam::Pluck mPluck;    
};

