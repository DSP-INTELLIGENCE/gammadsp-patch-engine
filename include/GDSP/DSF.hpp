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

class DSF : public Generator {
public:
    DSF(float freq = 440.f,
        float freqRatio = 1.f,
        float ampRatio = 0.5f,
        float harmonics = 8.f)
    : mOsc(freq, freqRatio, ampRatio, harmonics)
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

    void setFreqRatio(float r)
    {
        mOsc.freqRatio(r);
    }

    void setAmpRatio(float r)
    {
        mOsc.ampRatio(r);
    }

    void setHarmonics(float h)
    {
        mOsc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        mOsc.harmonicsMax();
    }

    void antialias()
    {
        mOsc.antialias();
    }

    float process() override
    {
        return mOsc();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::DSF<double> mOsc;
};



