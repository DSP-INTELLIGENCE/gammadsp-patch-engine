#pragma once
#include "ModComb.hpp"

class SlapbackComb : public ModComb
{
public:
    SlapbackComb(float maxDelay = 0.2f)
    : ModComb(maxDelay, 0.085f)
    {
        // Lock modulation
        setDM(0.0f);
        setFBM(0.0f);
        setFFM(0.0f);
        setAPM(0.0f);
        setMIXM(0.0f);
        setAM(1.0f);
        setIM(1.0f);

        // Slapback core
        setDelay(0.085f);       // 85 ms
        setFeedback(0.18f);     // light resonance
        setFeedforward(0.12f);  // adds attack snap
        setAllPass(0.05f);      // small room smear
        setMix(0.28f);
    }

    // Musical guardrails

    void setTime(float seconds)
    {
        seconds = std::clamp(seconds, 0.06f, 0.12f);
        setDelay(seconds);
    }

    void setResonance(float r)
    {
        r = std::clamp(r, 0.05f, 0.30f);
        setFeedback(r);
    }

    void setSnap(float s)
    {
        s = std::clamp(s, 0.0f, 0.25f);
        setFeedforward(s);
    }

    void setRoom(float a)
    {
        a = std::clamp(a, 0.0f, 0.20f);
        setAllPass(a);
    }

    void setWet(float m)
    {
        m = std::clamp(m, 0.15f, 0.35f);
        setMix(m);
    }
};
