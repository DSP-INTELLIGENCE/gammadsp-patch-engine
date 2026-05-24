#pragma once
#include "Biquad.hpp"

class HighShelfFilter : public Function
{
public:
    HighShelfFilter() {
        filter.setType(gam::HIGH_SHELF);
    }

    void  freq(float f)  { filter.freq(f); }
    float freq() const   { return filter.freq(); }

    void  res(float r)   { filter.res(r); }
    float res() const    { return filter.res(); }

    void  level(float v){ filter.level(v); }
    float level() const  { return filter.level(); }

    float process(float input) override {
        return filter(input);
    }

    void reset() {
        filter.reset();
    }

private:
    Biquad filter;
};
