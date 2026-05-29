#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModFilter.hpp"

class ModOnePole : public ModFilter1
{
public:

    ModOnePole(float initFreq = 1000.0f,
               gam::FilterType type = gam::FilterType::LOW_PASS,
               float stored = 0.0f)
    {
        setCutoff(initFreq);
        setAM(1.0f);
        setType(type);        
    }

    void setType(gam::FilterType t) { mFilter.type(t); }

    void lag(float seconds, float threshold = 0.001f)
    {
        mFilter.lag(seconds, threshold);
    }

    float process(float input) override
    {
        return update(mFilter, input);
    }

    float last() const { return mFilter.last(); }

private:
    gam::OnePole<double> mFilter;
};
