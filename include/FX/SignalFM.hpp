#pragma once
#include <algorithm>
#include <Gamma/Delay.h>
#include "Engine.hpp"
#include "Parameters.hpp"

class SignalFM : public Function
{
public:
    SignalFM(float maxDelay = 0.05f, float baseDelay = 0.005f)
    : delay(maxDelay), baseDelay(baseDelay)
    {
        setDepth(0.0f);
    }

    // -------- Controls --------
    void setBaseDelay(float d) { baseDelay = std::max(d, 0.0f); }
    void setDepth(float d)     { depth = d; }
    void setMM(float v)        { mm.set(v); }   // modulation signal

    float process(float input)
    {
        // integrate modulator -> phase warp
        phase += mm.process() * depth;

        float delayTime = baseDelay + phase;
        delayTime = std::clamp(delayTime, 0.0f, delay.maxDelay());

        delay.delay(delayTime);
        return delay(input);
    }

private:
    gam::Delay<float> delay;

    float baseDelay;
    float depth = 0.0f;
    float phase = 0.0f;

    Modulator mm;
};
