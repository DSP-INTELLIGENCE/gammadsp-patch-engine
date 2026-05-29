#pragma once
#include <algorithm>
#include "LFO.hpp"
#include "ModGenerator.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModLFO : public ModGenerator
{
public:
    ModLFO(float freq = 1.f, float phase = 0.f, float mod = 0.5f)
    {
        setFreq(freq);
        lfo.setPhase(phase);
        lfo.setMod(mod);

        setFM(0.0f);
        setPM(0.0f);
        setAM(1.0f);
        setRatio(1.0f);
        setIndex(1.0f);
    }

    void setFreq(float f)
    {
        baseFreq = std::max(0.001f, f);
        lfo.setFreq(baseFreq);
    }

    void setMod(float m)
    {
        lfo.setMod(std::clamp(m, 0.0f, 1.0f));
    }

    void setWave(LFO::Wave w) { lfo.setWave(w); }
    void setUnipolar(bool v)  { lfo.setUnipolar(v); }

    float process() override
    {
        return update(lfo);
    }

private:
    LFO lfo;
};
