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

class SineR : public Generator {
public:
    SineR(float freq = 440.0,
          float amp  = 1.0,
          float phase = 0.0)
    : mOsc(freq, amp, phase)
    {

    }

    void set(float f, float a, float p=0.0f)
    {
        mOsc.set(f, a, p);
    }

    void setFreq(float f)
    {
        mOsc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if(seconds > 0.0)
            mOsc.period(seconds);
    }

    void setAmp(float a)
    {
        mOsc.ampPhase(a, 0.0);
    }

    void setPhase(float p)
    {
        mOsc.ampPhase(amp(), p);
    }

    float freq() const
    {
        return mOsc.freq();
    }

    float amp() const
    {
        return mOsc.val;
    }
    float process()
    {
        return mOsc();
    }

    void reset()
    {
        mOsc.ampPhase(amp(), 0.0);
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

private:
    gam::SineR<double> mOsc;
};



