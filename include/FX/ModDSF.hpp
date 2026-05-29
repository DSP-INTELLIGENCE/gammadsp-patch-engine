#pragma once
#include <algorithm>
#include "ModGenerator.hpp"
#include "DSF.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModDSF : public ModGenerator
{
public:
    ModDSF(float freq = 440.f,
           float freqRatio = 1.f,
           float ampRatio = 0.5f,
           float harmonics = 8.f)
    {
        setFreq(freq);
        setFreqRatio(freqRatio);
        setAmpRatio(ampRatio);
        setHarmonics(harmonics);

        setFM(0.f);
        setPM(0.f);
        setAM(1.f);
        setRatio(1.f);
        setIndex(1.f);
    }

    void setFreq(float f)
    {
        baseFreq = f;          // 🔑 required for modulation math
        osc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if (seconds > 0.f)
            setFreq(1.f / seconds);
    }

    void setFreqRatio(float r)
    {
        osc.freqRatio(r);
    }

    void setAmpRatio(float r)
    {
        osc.ampRatio(r);
    }

    void setHarmonics(float h)
    {
        osc.harmonics(h);
    }

    void setHarmonicsMax()
    {
        osc.harmonicsMax();
    }

    void antialias()
    {
        osc.antialias();
    }

    float process() override
    {
        return update(osc);
    }

private:
    gam::DSF<double> osc;
};
