#pragma once
#include <cmath>

struct ClippingDetector
{
    bool clipped = false;

    inline void reset()
    {
        clipped = false;
    }

    inline float process(float x)
    {
        if (std::fabs(x) >= 1.f)
            clipped = true;
        return x;
    }
};
