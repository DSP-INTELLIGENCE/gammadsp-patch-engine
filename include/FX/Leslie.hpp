#pragma once
#include "FrequencyShifer.hpp"

class Leslie {
public:
    Leslie()
    : hornShifter(0.f), drumShifter(0.f)
    {
        setFast();
    }

    void setFast()  { hornTarget = 6.0f;  drumTarget = 5.0f; }
    void setSlow()  { hornTarget = 0.8f;  drumTarget = 0.6f; }
    void setStop()  { hornTarget = 0.0f;  drumTarget = 0.0f; }

    void process(float in, float& L, float& R)
    {
        updateRotor(hornSpeed, hornTarget, 0.02f);
        updateRotor(drumSpeed, drumTarget, 0.01f);

        float horn = hp.process(in);
        float drum = lp.process(in);

        processRotor(horn, hornPhase, hornSpeed, hornShifter, L, R, 0.7f);
        processRotor(drum, drumPhase, drumSpeed, drumShifter, L, R, 0.4f);

        float cab = 0.8f;
        L *= cab; R *= cab;
    }

private:
    void updateRotor(float& v, float target, float inertia)
    {
        v += (target - v) * inertia;
    }

    void processRotor(float x, float& phase, float speed,
                      FrequencyShifter& shifter,
                      float& L, float& R, float depth)
    {
        const float sr = gam::sampleRate();

        phase += 2.f * M_PI * speed / sr;
        if (phase >= 2.f * M_PI) phase -= 2.f * M_PI;

        float v = speed * std::cos(phase);
        shifter.setShift(v * 30.f); // scale

        float y = shifter.process(x);

        float g = 0.5f + 0.5f * std::sin(phase);
        float pan = std::sin(phase);

        L += y * g * (1 - pan);
        R += y * g * (1 + pan);
    }

private:
    float hornSpeed=0, drumSpeed=0;
    float hornTarget=0, drumTarget=0;

    float hornPhase=0, drumPhase=0;

    FrequencyShifter hornShifter;
    FrequencyShifter drumShifter;

    Biquad lp{800,0.7,gam::LOW_PASS};
    Biquad hp{800,0.7,gam::HIGH_PASS};
};
