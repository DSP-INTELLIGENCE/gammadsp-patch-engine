#pragma once
#include "DigitalDelay.hpp"

class SlapbackDelay : public DigitalDelay
{
public:
    SlapbackDelay(float maxDelay = 0.25f)
    : DigitalDelay(maxDelay, 0.09f)   // ~90 ms default
    {
        // Lock modulation
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);
        setIM(1.0f);
        setAM(1.0f);

        // Slapback character
        setDelay(0.09f);      // 90 ms
        setFeedback(0.05f);   // very low feedback
        setMix(0.22f);        // subtle thickening
    }

    // Musically constrained controls
    void setTime(float seconds)
    {
        seconds = std::clamp(seconds, 0.06f, 0.14f);
        setDelay(seconds);
    }

    void setWet(float m)
    {
        m = std::clamp(m, 0.10f, 0.35f);
        setMix(m);
    }

    void setRegen(float r)
    {
        r = std::clamp(r, 0.0f, 0.10f);
        setFeedback(r);
    }
};
