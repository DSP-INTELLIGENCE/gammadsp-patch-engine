#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "GDSP_ModAllPass2.hpp"
#include "GDSP_AllPass1.hpp"
#include "GDSP_OnePole.hpp"
#include "GroupDelayModulator.hpp"
#include "GDSP_ModDelay.hpp"

class GroupDelayReverb : public Function
{
public:
    GroupDelayReverb()
    : predelay(0.2f, 0.015f),     // 200ms max, 15ms default
      gd(14),
      air(8000.f, gam::LOW_PASS),
      body(120.f, gam::HIGH_PASS)
    {
        // Early diffusion
        for(auto& ap : early)
            ap.setFreq(1200.f);

        // Defaults
        setMix(0.35f);
        setSize(0.75f);
        setDecay(0.85f);
        setDamping(0.6f);
        setDispersion(0.8f);
        setMotion(0.6f);
    }

    // ---- User Controls ----
    void setMix(float v)        { mix = std::clamp(v, 0.f, 1.f); }
    void setSize(float v)       { size = std::clamp(v, 0.f, 1.f); }
    void setDecay(float v)      { decay = std::clamp(v, 0.f, 0.98f); }
    void setDamping(float v)    { damping = std::clamp(v, 0.f, 1.f); }
    void setDispersion(float v) { gd.setDispersion(v); }
    void setMotion(float v)
    {
        gd.setDepth(v);
        gd.setRate(0.02f + v * 0.1f);
    }

    float process(float x) override
    {
        // Pre-delay
        float in = predelay.process(x);

        // Early diffusion
        for(auto& ap : early)
            in = ap.process(in);

        // Dispersive space
        float space = gd.process(in + fb * decay);

        // Physical loss
        body.setFreq(50.f + size * 200.f);
        air.setFreq(2000.f + damping * 10000.f);

        space = body.process(space);
        space = air.process(space);

        fb = space;

        return x * (1.f - mix) + space * mix;
    }

private:
    ModDelay predelay;
    GroupDelayModulator gd;

    AllPass1 early[4];

    OnePole air;
    OnePole body;

    float fb = 0.f;

    float mix = 0.35f;
    float size = 0.75f;
    float decay = 0.85f;
    float damping = 0.6f;
};
