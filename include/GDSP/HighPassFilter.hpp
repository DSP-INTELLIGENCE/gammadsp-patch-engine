#pragma once
#include "Biquad.hpp"

class HighPassFilter: public Function
{
public:

    HighPassFilter() {
        filter.setType(gam::FilterType::HIGH_PASS);
    }
    void freq(float f) { filter.freq(f); }
    float freq()       { return filter.freq(); }
    void res(float r)  { filter.res(r); }
    float res()        { return filter.res(); }
    void level(float v) { filter.level(v); }
    float level() { return filter.level(); }

    float process(float input) override { return filter(input); }

protected:
    gam::Biquad<double> filter;
}