#pragma once
#include "MotionFilter.hpp"

class AutoWah : public MotionFilter
{
public:
    AutoWah()
    {
        setBaseCutoff(250.f);   // starting frequency
        setResonance(1.2f);     // vocal character
        setRate(0.15f);         // very slow LFO
        setDepth(0.2f);         // subtle LFO motion
        setEnvAmount(1.0f);     // envelope dominates
        setEnvTime(0.02f);      // fast envelope response
        setMix(1.0f);
    }

    // Musical macros
    void setSensitivity(float s)   { setEnvAmount(std::clamp(s, 0.f, 2.f)); }
    void setQ(float q)             { setResonance(std::clamp(q, 0.5f, 3.f)); }
    void setRange(float r)         { setDepth(std::clamp(r, 0.f, 1.f)); }

    float process(float x) { return MotionFilter::process(x); }
};
