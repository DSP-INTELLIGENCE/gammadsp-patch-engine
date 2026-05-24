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

class Celeste : public Function {
public:
    Celeste(float delay  = 0.04f,
            float amount = 0.004f,
            float freq   = 0.6f)
    {    
        mCeleste.delay(delay).amount(amount).freq(freq);
    }

    void setDelay(float seconds)
    {
        float mDelay = std::max(0.f, seconds);
        mCeleste.delay(mDelay);
    }

    void setAmount(float seconds)
    {
        float mAmount = std::max(0.f, seconds);
        mCeleste.amount(mAmount);
    }

    void setFreq(float hz)
    {
        float mFreq = std::max(0.f, hz);
        mCeleste.freq(mFreq);
    }

    float process(float input) override
    {
        return mCeleste(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    gam::Celeste<double> mCeleste;    
};


