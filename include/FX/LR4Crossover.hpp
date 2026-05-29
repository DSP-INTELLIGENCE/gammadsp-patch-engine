#pragma once

class LR4Crossover
{
public:
    LR4Crossover(float cutoff = 1000.0f)
    {
        setCutoff(cutoff);
    }

    void setCutoff(float fc)
    {
        mCutoff = fc;

        constexpr float lr4Q = 0.5411961f;

        setupStage(lp1, gam::LOW_PASS,  fc, lr4Q);
        setupStage(lp2, gam::LOW_PASS,  fc, lr4Q);
        setupStage(hp1, gam::HIGH_PASS, fc, lr4Q);
        setupStage(hp2, gam::HIGH_PASS, fc, lr4Q);
    }

    void reset()
    {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
    }

    inline void process(float input, float& low, float& high)
    {
        low  = lp2.process(lp1.process(input));
        high = hp2.process(hp1.process(input));
    }

    float cutoff() const { return mCutoff; }

private:
    static inline void setupStage(Biquad& b,
                                  gam::FilterType type,
                                  float freq,
                                  float Q)
    {
        b.setType(type);
        b.setFreq(freq);
        b.setRes(Q);
    }

    float mCutoff;
    Biquad lp1, lp2;
    Biquad hp1, hp2;
};
