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

class Osc : public Generator {
public:
    Osc(float freq = 440.f,
        float phase = 0.f,
        unsigned tableSize = 512)
    : mOsc(freq, phase, tableSize)
    {
    
    }

    void setFreq(float f) { mOsc.freq(f); } 
    void setPeriod(float seconds) {
        if(seconds > 0.f)
            mOsc.freq(1.f / seconds);
    }   
    void setPhase(float p) { mOsc.phase(p); }
    float freq() const { return mOsc.freq(); }
    float phase() const { return mOsc.phase(); }
    void clearTable()
    {
        for(unsigned i = 0; i < mOsc.size(); ++i)
            mOsc[i] = 0.f;
    }
    void addSine(float cycles, float amplitude, float phase)
    {
        mOsc.addSine(cycles, amplitude, phase);
    }
    float getTableSample(unsigned index) const
    {
        if(index < mOsc.size())
            return mOsc[index];
        return 0.f;
    }

    void setTableSample(unsigned index, float value)
    {
        if(index < mOsc.size())
            mOsc[index] = value;
    }

    unsigned tableSize() const { return mOsc.size(); }

    float tick() { return mOsc(); }
    void reset() { mOsc.reset(); }

    void run(float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process();
    }
    
    float process() override
    {
        return mOsc();
    }

    void phaseAdd(float p) { mOsc.phaseAdd(p); }
    void freq(float f) { mOsc.freq(f); }

private:
    gam::Osc<double> mOsc;
};



