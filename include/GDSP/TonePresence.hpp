#pragma once
#include "OnePole.hpp"
#include "Biquad.hpp"

class TonePresence: public Function
{
public:
    
    TonePresence()
    {
        // Tone filters (tilt EQ)
        toneLow.setType(gam::LOW_PASS);
        toneLow.setFreq(1200.0f);

        toneHigh.setType(gam::HIGH_PASS);
        toneHigh.setFreq(1200.0f);

        // Presence filter
        presence.setType(gam::BAND_PASS);
        presence.setFreq(3500.0f);
        presence.setRes(0.6f);
        presence.setLevel(1.0f);
    }

    void setTone(float t)
    {
        // t = 0..1
        tone = std::clamp(t, 0.f, 1.f);
    }

    void setPresence(float p)
    {
        presenceAmt = std::clamp(p, 0.f, 1.f);
    }

    float process(float x) override
    {
        // --- Tone tilt EQ ---
        float low  = toneLow.process(x);
        float high = toneHigh.process(x);

        float tilt = (1.f - tone) * low + tone * high;

        // --- Presence lift ---
        float pres = presence.process(tilt);
        return tilt + pres * presenceAmt;
    }

private:
    float tone = 0.5f;
    float presenceAmt = 0.0f;

    OnePole toneLow;
    OnePole toneHigh;
    Biquad  presence;
};
