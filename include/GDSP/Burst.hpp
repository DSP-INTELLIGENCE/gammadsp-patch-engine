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

class Burst : public Generator {
public:
    Burst(float startFreq = 20000.f,
          float endFreq   = 4000.f,
          float decay     = 0.1f,
          float resonance = 2.0f)
    : mBurst(startFreq, endFreq, decay, resonance)
    {
        
    }

    void trigger(float startFreq,
                        float endFreq,
                        float decay,
                        bool resetState=true)
    {
        mBurst(startFreq, endFreq, decay, resetState);
    }

    void reset()
    {
        mBurst.reset();
    }

    float process() override
    {
        return mBurst();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::Burst mBurst;
    
};

