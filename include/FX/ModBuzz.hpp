#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>
#include "Buzz.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"
#include "ModGenerator.hpp"

class ModBuzz : public ModGenerator
{
public:
    ModBuzz(float freq = 440.f,
            float phase = 0.f,
            float harmonics = 8.f)
    {
        setFreq(freq);
        setHarmonics(harmonics);

        setFM(0.f);
        setPM(0.f);
        setAM(1.f);
        setRatio(1.f);
        setIndex(1.f);
    }

    void setFreq(float f)
    {
        baseFreq = f;            // 🔑 required for modulation math
        osc.freq(f);
    }

    void setPeriod(float seconds)
    {
        if (seconds > 0.f)
            setFreq(1.f / seconds);
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

    void normalize(bool enable)
    {
        osc.normalize(enable);
    }

    float process() override
    {
        return update(osc);
    }

private:
    gam::Buzz<double> osc;
};
