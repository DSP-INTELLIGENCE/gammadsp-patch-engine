#pragma once
#include <algorithm>
#include "ModGenerator.hpp"
#include "Sweep.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModSweep: public ModGenerator
{
public:

    void setFreq(float v) { baseFreq = v; }
    float process() override
    {
        return update(_phasor);
    }
protected:

    gam::Sweep<> _phasor;
};