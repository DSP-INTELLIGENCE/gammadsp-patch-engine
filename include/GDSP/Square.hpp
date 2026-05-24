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

class Square : public Generator {
public:
    Square(float freq = 440.f,
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

    float process()
    {
        return mOsc(integrate);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Square<double> mOsc;
    float integrate = 0.999f;
};
