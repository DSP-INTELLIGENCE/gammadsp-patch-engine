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

class Sweep : public Generator {
public:
    Sweep(float freq = 1.0f,
          float phase = 0.0f)
    : mSweep(freq, phase)
    {
        
    }

    void setFreq(float f)
    {
        mSweep.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.f)
            mSweep.freq(1.f / seconds);
    }

    void setPhase(float p)
    {
        mSweep.phase(p);
    }

    float process() override
    {
        return mSweep();
    }

    float processFM(float freqOffset)
    {
        return mSweep(freqOffset);
    }

    void reset()
    {
        mSweep.reset();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Sweep<> mSweep;
};




