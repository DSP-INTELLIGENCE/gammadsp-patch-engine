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

class MonoSynth : public Generator {
public:
    MonoSynth(float freq  = 440.f,
              float dur   = 0.8f,
              float ctf1  = 1000.f,
              float ctf2  = 100.f,
              float res   = 3.0f)
    : 
    mSynth(freq, dur, ctf1, ctf2, res)
    {
        
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(1.f, hz);
        mSynth.freq(mFreq);
    }

    void setCutoff(float fc)
    {
        mSynth.filter.freq(fc);    
    }

    void reset()
    {
        mSynth.reset();
    }

    float process() override
    {
        return mSynth();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::MonoSynth mSynth;    
};

