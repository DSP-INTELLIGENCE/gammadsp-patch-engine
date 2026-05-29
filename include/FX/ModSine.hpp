#pragma once
#include <algorithm>
#include "ModGenerator.hpp"
#include "Sine.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModSine : public ModGenerator
{
public:
    ModSine(float f)
    {
        setFreq(f);
        setFM(0);
        setPM(0);
        setAM(1);
        setRatio(1);
        setIndex(1);
    }

    void setFreq(float f)
    {
        baseFreq = f;        // 🔑 always store base frequency
        mOsc.freq(f);
    }

    float process() override
    {
        return update(mOsc);
    }

private:
    gam::Sine<double> mOsc;
};
