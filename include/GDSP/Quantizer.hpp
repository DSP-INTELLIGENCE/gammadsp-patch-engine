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

class Quantizer : public Function {
public:
    Quantizer(float freq = 2000.f,
              float step = 0.f)
    : 
    mQuant(freq, step)
    {

    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.1f, hz);
        mQuant.freq(mFreq);
    }

    void setPeriod(float seconds)
    {
        mQuant.period(std::max(1e-6f, seconds));
    }

    void setStep(float step)
    {
        float mStep = std::max(0.f, step);
        mQuant.step(mStep);
    }

    float process(float input) override
    {
        return mQuant(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Quantizer<double> mQuant;    
};
