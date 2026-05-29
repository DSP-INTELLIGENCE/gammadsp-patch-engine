#pragma once
#include "Biquad.hpp"

struct CrossoverBand
{
    Biquad lp1, lp2;
    Biquad hp1, hp2;

    void setCutoff(float freq)
    {
        lp1.setType(gam::LOW_PASS);  lp2.setType(gam::LOW_PASS);
        hp1.setType(gam::HIGH_PASS); hp2.setType(gam::HIGH_PASS);

        lp1.setFreq(freq); lp2.setFreq(freq);
        hp1.setFreq(freq); hp2.setFreq(freq);

        lp1.setRes(0.707f); lp2.setRes(0.707f);
        hp1.setRes(0.707f); hp2.setRes(0.707f);
    }

    void process(float x, float& low, float& high)
    {
        low  = lp2.process(lp1.process(x));
        high = hp2.process(hp1.process(x));
    }
};
