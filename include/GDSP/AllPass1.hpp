#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"

class AllPass1 : public Function {
public:
    AllPass1(float initFreq = 1000.0f)
    : mFilter(initFreq)
    {
        
        freq(std::max(1.f, initFreq));    
    }

    void freq(float freqHz)
    {
        mFilter.freq(std::max(1.f, freqHz));
    }

    void reset()
    {
        mFilter.reset();
    }

    float high(float input)
    {
        return mFilter.high(input);
    }

    float low(float input)
    {
        return mFilter.low(input);
    }
        
    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::AllPass1<double> mFilter;
};

