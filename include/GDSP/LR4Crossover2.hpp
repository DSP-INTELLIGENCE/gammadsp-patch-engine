#pragma once
#include "GDSP_Filters.hpp" // wherever Biquad lives

class LR4Crossover2
{
public:
    void prepare()
    {
        // Butterworth Q for LR4 = 0.707 on each 2nd-order stage
        lp1.setType(gam::LOW_PASS);  lp1.setRes(0.707f);
        lp2.setType(gam::LOW_PASS);  lp2.setRes(0.707f);

        hp1.setType(gam::HIGH_PASS); hp1.setRes(0.707f);
        hp2.setType(gam::HIGH_PASS); hp2.setRes(0.707f);

        setFreq(freqHz);
    }

    void setFreq(float f)
    {
        float fs = (float)gam::sampleRate();
        freqHz = std::clamp(f, 20.f, 0.45f * fs);

        lp1.setFreq(freqHz); lp2.setFreq(freqHz);
        hp1.setFreq(freqHz); hp2.setFreq(freqHz);
    }

    void reset()
    {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
    }

    inline void process(float x, float& low, float& high)
    {
        float l = lp2.process(lp1.process(x));
        float h = hp2.process(hp1.process(x));
        low  = l;
        high = h;
    }

private:
    float freqHz = 800.f;
    Biquad lp1, lp2, hp1, hp2;
};
