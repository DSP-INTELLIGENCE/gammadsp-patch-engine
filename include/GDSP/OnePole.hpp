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

class OnePole : public Function {
public:
    
    OnePole(float initFreq = 1000.0f,
            gam::FilterType type = gam::LOW_PASS,
            float stored = 0.0f)
    {
        setFreq(initFreq);
        setType(type);
    }

    void setFreq(float freqHz)
    {
        mFilter.freq(freqHz);
    }

    void setType(gam::FilterType type)
    {    
        mFilter.type(type);
    }

    void lag(float lengthSeconds, float threshold=0.001f)
    {
        mFilter.lag(lengthSeconds, threshold);
    }

    void reset(float value=0.0f)
    {
        mFilter.reset(value);
    }

    float last() const
    {
        return mFilter.last();
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

    float getFreq() { return mFilter.freq(); }

private:
    gam::OnePole<double> mFilter;    
};

