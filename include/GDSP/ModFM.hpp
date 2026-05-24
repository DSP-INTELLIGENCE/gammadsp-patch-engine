#pragma once
#include <algorithm>
#include "Engine.hpp"
#include "Parameters.hpp"
#include <Gamma/Delay.h>

class ModFM : public Function
{
public:
    ModFM(float maxDelay = 0.05f, float baseDelay = 0.005f)
    : delay(maxDelay), baseDelay(baseDelay)
    {
        setDepth(0.0f);
        setFB(0.0f);
        setIM(1.0f);
        setAM(1.0f);
    }

    // -------- Controls --------
    void setBaseDelay(float d) { baseDelay = std::max(d, 0.0f); }
    void setDepth(float d)     { depth = std::max(d, 0.0f); }
    void setFB(float f)        { fb = std::clamp(f, 0.0f, 0.99f); }

    void setMM(float v) { mm.set(v); }   // modulation signal
    void setIM(float v) { im.set(v); }   // input gain
    void setAM(float v) { am.set(v); }   // output gain

    float process(float input)
    {
        float m  = mm.process();           // [-1,1]
        float in = input * im.process();

        float delayTime = baseDelay + depth * m;
        delayTime = std::clamp(delayTime, 0.0f, delay.maxDelay());

        delay.delay(delayTime);

        float y = delay(in + fb * last);

        last = y;
        return y * am.process();
    }

private:
    gam::Delay<float> delay;

    float baseDelay;
    float depth = 0.0f;
    float fb    = 0.0f;

    Modulator mm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};
