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

class FreqShift : public Function {
public:
    FreqShift(float shiftHz = 0.0f)
    : 
    mShift(shiftHz)
    {
        
    }

    void setShift(float shiftHz)
    {    
        mShift.freq(shiftHz);
    }

    float process(float input) override
    {
        return mShift(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:

    gam::FreqShift<double> mShift;
};

