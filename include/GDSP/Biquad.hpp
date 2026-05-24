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

class Biquad : public Function
{
public:
    using FilterType = gam::FilterType;

    Biquad(float initFreq = 1000.0f,
           float initRes  = 0.707f,           
           float initLevel = 1.0f,
           FilterType type = gam::LOW_PASS)
    {
        mFilter.set(initFreq, initRes, type);
        mFilter.level(initLevel);                
    }

    void setFreq(float f)    { mFilter.freq(f); }
    void setRes(float r)     { mFilter.res(r); }
    void setLevel(float l)   { mFilter.level(l); }
    void setType(FilterType t){ mFilter.type(t); }
    void setCoef(float a0, float a1, float a2, float b1, float b2)
    { mFilter.coef(a0, a1, a2, b1, b2); }

    void reset() { mFilter.reset(); }

    float getFreq()  const { return mFilter.freq(); }
    float getRes()   const { return mFilter.res(); }
    float getLevel() const { return mFilter.level(); }

    float process(float input) override
    {
        return mFilter(input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float freq() { return mFilter.freq(); }
    void  freq(float f) { mFilter.freq(f); }
    float  res() { return mFilter.res(); }
    void  res(float r) { mFilter.res(r); }
private:
    gam::Biquad<double> mFilter;
    
};






