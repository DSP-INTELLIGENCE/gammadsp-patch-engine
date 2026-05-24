#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"
#include "GDSP_Delay.hpp"

// Simple radiation model (high-pass differentiator)
class PipeRadiation {
public:
    void set(float a, float g) {
        alpha = std::clamp(a, 0.f, 0.9999f);
        gain  = std::max(0.f, g);
    }

    float process(float x) {
        float y = x - alpha * z;
        z = x;
        return y * gain;
    }

private:
    float z = 0.f;
    float alpha = 0.95f;
    float gain  = 0.5f;
};

class TubePipeFilter : public Function
{
public:
    TubePipeFilter(float maxDelay = 2.f)
    : delay(maxDelay, 0.02f)
    {
        setLength(0.6f);
        setLoss(0.25f);
        setNonlinearity(0.2f);
        setFeedback(0.95f);
        setMix(1.0f);
        rad.set(0.96f, 0.6f);

        setIM(1.f);
        setAM(1.f);
    }

    // -------- Physical controls --------
    void setLength(float meters)
    {
        meters = std::clamp(meters, 0.1f, 4.f);
        float c = 343.f; // speed of sound
        float delayTime = meters / c;
        delay.setDelay(delayTime);
    }

    void setLoss(float l)        { loss = std::clamp(l, 0.f, 1.f); }
    void setNonlinearity(float n){ nonlin = std::clamp(n, 0.f, 1.f); }
    void setFeedback(float f)    { fb = std::clamp(f, 0.f, 0.999f); }
    void setMix(float m)         { mix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        // feedback air column
        float y = delay.read();

        // tube losses
        y *= (1.f - loss * 0.15f);

        // airflow nonlinearity
        y = std::tanh(y * (1.f + nonlin * 4.f));

        // reinject
        delay.write(in + y * fb);

        // radiation at opening
        float out = rad.process(y);

        out = x * (1.f - mix) + out * mix;
        out *= am.process();
        return out;
    }

private:
    Delay delay;
    PipeRadiation rad;

    float loss = 0.25f;
    float nonlin = 0.2f;
    float fb = 0.95f;
    float mix = 1.f;

    Modulator im, am;
};
