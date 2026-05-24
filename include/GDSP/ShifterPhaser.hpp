#pragma once
#include "FrequencyShifer.hpp"

class ShifterPhaser : public Function {
public:
    ShifterPhaser(int stages = 6,
                  float apFreq = 800.f,
                  float shiftHz = 20.f)
    : ap(apFreq, stages),
      shifter(shiftHz)
    {
        setMix(0.5f);
        setFeedback(0.3f);
    }

    void setAllpassFreq(float f) { ap.freq(f); }
    void setShift(float hz)      { shifter.setShift(hz); }

    void setMix(float m)         { mix = std::clamp(m, 0.f, 1.f); }
    void setFeedback(float f)    { fb  = std::clamp(f, -0.99f, 0.99f); }

    void reset()
    {
        ap.reset();
        shifter.reset();
        last = 0.f;
    }

    float process(float x) override
    {
        float in = x + last * fb;

        float y = ap.process(in);
        y = shifter.process(y);

        float out = x * (1.f - mix) + y * mix;

        last = y;
        return out;
    }

private:
    AllPass1Block ap;
    FrequencyShifter shifter;

    float mix = 0.5f;
    float fb  = 0.3f;
    float last = 0.f;
};
