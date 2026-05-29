#pragma once
#include "FX/AnalogDelay.hpp"

class AnalogEcho : public AnalogDelay
{
public:
    AnalogEcho(float maxDelay = 2.5f)
    : AnalogDelay(maxDelay, 0.38f)
    {
        // Lock most modulation for stability
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);
        setIM(1.0f);
        setAM(1.0f);

        // Classic echo defaults
        setDelay(0.38f);       // 380 ms
        setFeedback(0.42f);    // musical tail
        setMix(0.32f);         // present but not washed
        setTone(0.55f);        // mild darkening
        setDrive(0.6f);        // subtle analog compression
        setAsym(0.1f);         // warmth
    }

    // ---- Musical control ranges ----

    void setTime(float seconds)
    {
        seconds = std::clamp(seconds, 0.20f, 0.60f);
        setDelay(seconds);
    }

    void setRegen(float r)
    {
        r = std::clamp(r, 0.20f, 0.65f);
        setFeedback(r);
    }

    void setWet(float m)
    {
        m = std::clamp(m, 0.20f, 0.45f);
        setMix(m);
    }

    void setColor(float c)
    {
        c = std::clamp(c, 0.0f, 1.0f);
        setTone(0.35f + c * 0.45f);   // maps to dark → bright echo
        setDrive(0.3f + c * 0.7f);
    }
};
