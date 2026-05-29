#pragma once
#include <algorithm>
#include "ModGenerator.hpp"
#include "AccumPhase.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModPhasor: public ModGenerator
{
public:

    void setFreq(float v) { baseFreq = v; }
    float process() override
    {
        return update(_phasor);
    }

private:

    AccumPhase _phasor;
};
