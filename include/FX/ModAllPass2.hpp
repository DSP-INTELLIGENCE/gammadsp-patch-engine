#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModFilter.hpp"

class ModAllPass2 : public ModFilter2
{
public:
    ModAllPass2(float initFreq = 1000.0f, float initWidth = 100.0f)
    {
        setCutoff(initFreq);
        setWidth(initWidth);
        setAM(1.0f);
    }

    float process(float input) override
    {
        // reuse ModFilter's modulation & safety path
        return update(mFilter, input);
    }

    
    void reset() { mFilter.reset(); }

private:
    gam::AllPass2<double> mFilter;
};
