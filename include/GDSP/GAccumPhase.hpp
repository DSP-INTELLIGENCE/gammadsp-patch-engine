#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>  // <-- add
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"

class AccumPhase : public TGenerator<float> {
public:
    AccumPhase(float freq = 440.f,
               float phase = 0.f,
               float radius = 3.14159265358979323846f)
    : mAccum(freq, phase, radius)
    {}

    float nextPhase() { return mAccum.nextPhase(); }
    float nextPhase(float frqOffset) { return mAccum.nextPhase(frqOffset); }

    void freqAdd(float v) { mAccum.freqAdd(v); }
    void freqMul(float v) { mAccum.freqMul(v); }
    void phaseAdd(float v) { mAccum.phaseAdd(v); }
    void phaseSub(float v) { mAccum.phaseAdd(-v); }
    float freqUnit() const { return mAccum.freqUnit(); }

    void setFreq(float f) { mAccum.freq(std::max(0.0f, f)); } // <-- change

    void setPeriod(float seconds) {
        if(seconds > 0.f) mAccum.period(seconds);
    }

    void setPhase(float p) { mAccum.phase(p); }
    void addPhase(float d) { mAccum.phaseAdd(d); }
    void setRadius(float r) { mAccum.radius(r); }

    float freq() const { return mAccum.freq(); }
    float period() const { return mAccum.period(); }
    float phase() const { return mAccum.phase(); }
    float radius() const { return mAccum.radius(); }

    float process() noexcept override final { 
        float p = mAccum.nextPhase(); 
        if(p_waveshaper) return p_waveshaper->process(p);
        return p;
    }
    
    void run(float* output, size_t n) {
        for(size_t i = 0; i < n; ++i) output[i] = process();
    }

    void freq(float v) { mAccum.freq(std::max(0.0f, v)); } // <-- change

    float operator()() { 
        return process();
    }
    float operator()(float v) { 
        float p = mAccum.nextPhase(v); 
        if(p_waveshaper) return p_waveshaper->process(p);
        return p;
    }


    TFunction<float> * p_waveshaper = nullptr;

private:
    gam::AccumPhase<double> mAccum;
};
