#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModFilter.hpp"

class ModBiquad : public ModFilter
{
public:
    using FilterType = gam::FilterType;

    ModBiquad(float initFreq = 1000.0f,
           float initRes  = 0.707f,
           FilterType type = gam::LOW_PASS,
           float initLevel = 1.0f)
    {
        setCutoff(initFreq);
        setRes(initRes);
        mFilter.level(initLevel);
        setAM(1.0f);
        setIM(1.0f);
        setFBM(0.0f);
    }

    void setType(FilterType t){ mFilter.type(t); }
    void setCoef(float a0, float a1, float a2, float b1, float b2) { mFilter.coef(a0, a1, a2, b1, b2); }
    void reset() { mFilter.reset(); }
    
    float getFreq()  const { return baseCutoff; }
    float getRes()   const { return baseRes; }
    float getLevel() const { return mFilter.level(); }

    float process(float input)
    {
        return update(mFilter, input);
    }

    void run(const float* input, float* output, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    
private:
    gam::Biquad<double> mFilter;
};

