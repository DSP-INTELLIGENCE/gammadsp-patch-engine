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

class Reson : public Function {
public:
    Reson(float initFreq = 1000.0f,
          float initWidth = 100.0f)
    : mFilter(initFreq, initWidth)
    {
        
    }

    void reset()
    {
        mFilter.reset();
    }

    void setFreq(float freqHz)
    {
        mFilter.freq(std::clamp(freqHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    void setWidth(float widthHz)
    {
        mFilter.width(std::clamp(widthHz, 0.f, (float)gam::sampleRate()*0.5f));    
    }

    float process(float input)
    {
        return mFilter(input);
    }

    void run(const float * input, float * output, size_t n)
    {
        for(size_t i = 0; i < n; i++)
            output[i] = process(input[i]);    
    }

private:
    gam::Reson<double> mFilter;
    
};




