#pragma once
#include "Biquad.hpp"

class LowPassFilter : public Function
{
public:
    LowPassFilter() {
        filter.setType(gam::LOW_PASS);
    }

    void freq(float f)  { filter.freq(f); }
    float freq() const  { return filter.freq(); }

    void res(float r)   { filter.res(r); }
    float res() const   { return filter.res(); }

    void level(float v) { filter.level(v); }
    float level() const { return filter.level(); }

    float process(float input) override {
        return filter.process(input);
    }

    void reset() {
        filter.reset();
    }

private:
    gam::Biquad<double> filter;
};
