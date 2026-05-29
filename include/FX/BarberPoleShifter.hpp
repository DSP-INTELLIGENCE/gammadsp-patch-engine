#pragma once
#include "FrequencyShifer.hpp"

class BarberPoleShifter : public Function {
public:
    BarberPoleShifter(float shiftHz = 50.f, float rateHz = 0.1f)
    : mShift(shiftHz), mRate(rateHz),
      up( shiftHz),
      down(-shiftHz)
    {
        reset();
    }

    void setShift(float hz)
    {
        mShift = hz;
        up.setShift( hz);
        down.setShift(-hz);
    }

    void setRate(float hz) { mRate = hz; }

    void reset()
    {
        phase = 0.f;
        up.reset();
        down.reset();
    }

    float process(float x) override
    {
        const float sr = gam::sampleRate();

        // advance crossfade LFO
        phase += 2.f * M_PI * mRate / sr;
        if (phase >= 2.f * M_PI) phase -= 2.f * M_PI;

        float fadeUp   = 0.5f * (1.f + std::sin(phase));
        float fadeDown = 1.f - fadeUp;

        float u = up.process(x);
        float d = down.process(x);

        return u * fadeUp + d * fadeDown;
    }

private:
    float mShift;
    float mRate;
    float phase = 0.f;

    FrequencyShifter up;
    FrequencyShifter down;

};
