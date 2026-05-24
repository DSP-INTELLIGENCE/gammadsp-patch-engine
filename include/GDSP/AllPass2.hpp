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

class AllPass2 : public Function {
public:
    AllPass2(float initFreq = 1000.0f,
             float initWidth = 100.0f)
    : mFilter(initFreq, initWidth)
    {
        
    }

    void freq(float freqHz)
    {
        mFilter.freq(std::max(1.f, freqHz));    
    }

    void width(float widthHz)
    {
        mFilter.width(std::max(0.f, widthHz));    
    }

    void reset()
    {
        mFilter.reset();
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
    gam::AllPass2<double> mFilter;    
};



