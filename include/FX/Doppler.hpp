#pragma once
#include "FrequencyShifer.hpp"

class Doppler : public Function {
public:
    Doppler(float radius = 1.0f, float speed = 1.0f)
    : shifter(0.f)
    {
        setRadius(radius);
        setSpeed(speed);
    }

    void setRadius(float r) { R = std::max(0.1f, r); }
    void setSpeed(float s)  { speed = s; }

    void reset()
    {
        phase = 0.f;
        shifter.reset();
        delay.reset();
    }

    float process(float x) override
    {
        const float sr = gam::sampleRate();
        const float c = 343.0f; // speed of sound (m/s)

        phase += 2.f * M_PI * speed / sr;
        if (phase >= 2.f * M_PI) phase -= 2.f * M_PI;

        float v = speed * R * std::cos(phase);   // radial velocity
        float shift = (v / c) * 1000.f;          // perceptual scale

        shifter.setShift(shift);

        float d = std::sqrt(R*R + 1.f - 2.f * R * std::cos(phase));
        float gain = 1.f / d;

        delay.setDelay(0.002f + 0.003f * d);     // time-of-flight

        float y = shifter.process(x);
        y = delay.process(y);

        return y * gain;
    }

private:
    float R = 1.f;
    float speed = 1.f;
    float phase = 0.f;

    FrequencyShifter shifter;
    DelayLine delay;
};
