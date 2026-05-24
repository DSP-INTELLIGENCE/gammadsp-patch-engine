#pragma once

class LR2Crossover
{
public:
    LR2Crossover(float cutoff = 1000.0f)
    {
        setCutoff(cutoff);
    }

    void setCutoff(float fc)
    {
        mCutoff = fc;

        lp.setType(gam::LOW_PASS);
        hp.setType(gam::HIGH_PASS);

        lp.setFreq(fc);
        hp.setFreq(fc);

        constexpr float butterQ = 0.70710678f;

        lp.setRes(butterQ);
        hp.setRes(butterQ);
    }

    void reset()
    {
        lp.reset();
        hp.reset();
    }

    inline void process(float input, float& low, float& high)
    {
        low  = lp.process(input);
        high = hp.process(input);
    }

    float cutoff() const { return mCutoff; }

private:
    float  mCutoff;
    Biquad lp;
    Biquad hp;
};
