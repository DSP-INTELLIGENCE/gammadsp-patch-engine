#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModFilter.hpp"

class ModNotch : public ModFilter2
{
public:
    ModNotch(float initFreq = 1000.0f, float initWidth = 100.0f)
    {
        setCutoff(initFreq);
        setWidth(initWidth);
        setAM(1.0f);
    }

    float process(float input) override
    {
        return update(mFilter, input);
    }

    void reset() { mFilter.reset(); }

private:
    gam::Notch<double> mFilter;
};
