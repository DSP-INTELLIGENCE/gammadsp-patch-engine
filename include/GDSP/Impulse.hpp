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

class Impulse : public Generator {
public:
    Impulse(float freq = 440.f,
            float phase = 0.f)
    : mOsc(freq, phase)
    {
        
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }

    float process() override
    {
        return mOsc();
    }

    float saw()
    {
        return mOsc.saw(integrate);
    }

    float square()
    {
        return mOsc.square(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Impulse<double> mOsc;
    float integrate = 0.999f;
};


