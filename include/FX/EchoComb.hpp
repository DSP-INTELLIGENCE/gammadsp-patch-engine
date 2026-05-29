#pragma once
#include "ModComb.hpp"

class EchoComb : public ModComb
{
public:
    EchoComb(float maxDelay = 3.0f, float initDelay=0.5f)
    : ModComb(maxDelay, initDelay)
    {
        // Lock modulation for stability
        setDM(0.0f);
        setFBM(0.0f);
        setFFM(0.0f);
        setAPM(0.0f);
        setMIXM(0.0f);
        setAM(1.0f);
        setIM(1.0f);

        // Echo + body defaults
        setDelay(initDelay);
        setFeedback(0.48f);      // long resonant tail
        setFeedforward(0.22f);   // attack clarity
        setAllPass(0.18f);       // spatial smear
        setMix(0.35f);
    }

    // --- Musical Controls ---

    void setTime(float seconds)
    {
        seconds = std::clamp(seconds, 0.22f, 0.65f);
        setDelay(seconds);
    }

    void setRegen(float r)
    {
        r = std::clamp(r, 0.35f, 0.75f);
        setFeedback(r);
    }

    void setBody(float b)
    {
        b = std::clamp(b, 0.10f, 0.40f);
        setFeedforward(b);
    }

    void setSpace(float s)
    {
        s = std::clamp(s, 0.10f, 0.35f);
        setAllPass(s);
    }

    void setWet(float m)
    {
        m = std::clamp(m, 0.20f, 0.45f);
        setMix(m);
    }
};
