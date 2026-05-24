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

class AccumPhase : public Generator {
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

    void freq(float f) { mAccum.freq(f); }
    void setFreq(float f) { mAccum.freq(std::max(0.0f, f)); } // <-- change

    void period(float p) { mAccum.period(p); }
    void setPeriod(float seconds) {
        if(seconds > 0.f) mAccum.period(seconds);
    }

    void phase(float p) { mAccum.phase(p); }
    void setPhase(float p) { mAccum.phase(p); }
    void addPhase(float d) { mAccum.phaseAdd(d); }
    void radius(float r) { mAccum.radius(r); }
    void setRadius(float r) { mAccum.radius(r); }

    float freq() const { return mAccum.freq(); }
    float period() const { return mAccum.period(); }
    float phase() const { return mAccum.phase(); }
    float radius() const { return mAccum.radius(); }

    float process() override { return mAccum.nextPhase(); }
    float process(float freqOffset) { return mAccum.nextPhase(freqOffset); }
    
    void run(float* output, size_t n) {
        for(size_t i = 0; i < n; ++i) output[i] = process();
    }

    void freq(float v) { mAccum.freq(std::max(0.0f, v)); } // <-- change

    float operator()() { return mAccum.nextPhase(); }
    float operator()(float v) { return mAccum.nextPhase(v); }

private:
    gam::AccumPhase<double> mAccum;
};


