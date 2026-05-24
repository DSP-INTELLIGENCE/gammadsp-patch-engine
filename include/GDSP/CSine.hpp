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

class CSine : public Generator {
public:
    CSine(float _freq = 440.f,
          float _amp  = 1.f,
          float _decay60 = -1.f,
          float _phase = 0.f)
    : mOsc(_freq, _amp, _decay60, _phase)
    {
        
    }


    void set(float f, float p, float a, float d=-1.f)
    {    
        mOsc.freq(f);    
        mOsc.amp(a);
        mOsc.decay(d);
        mOsc.phase(p);
    }

    void setFreq(float f)    { mOsc.freq(f); }
    void setPhase(float p)   { mOsc.phase(p); }
    void setAmp(float a)     { mOsc.amp(a);}
    void setDecay(float d)   { mOsc.decay(d);}
    float getFreq() const    { return mOsc.freq(); }
    float getAmp() const     { return mOsc.amp(); }
    float getDecay() const   { return mOsc.decay(); }

    float process() override
    {
        auto c = mOsc();
        return static_cast<float>(c.r);
    }

    float processImag()
    {
        auto c = mOsc.val;
        return static_cast<float>(c.i);
    }

    void reset()
    {
        mOsc.reset();
    }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }

    
private:
    gam::CSine<double> mOsc;
};






