#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModFilter.hpp"

class ModAllPass1 : public ModFilter1
{
public:

    ModAllPass1(float initFreq = 1000.0f)
    {        
        setCutoff(initFreq);
        setAM(1.0f);
    }   

    float process(float input) override
    {
        return update(mFilter, input);
    }
    
    void reset()        { mFilter.reset(); }

    float high(float x) { return mFilter.high(x); }
    float low (float x) { return mFilter.low(x); }

    float getFreq() { return mFilter.freq(); }

private:
    
    gam::AllPass1<double> mFilter;
};
