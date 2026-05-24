#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <vector>
#include <memory>
#include <atomic>
#include "Engine.hpp"
#include "Parameters.hpp"

class Sine : public Generator
{
public:
    Sine(float f)
    {
        setFreq(f);        
    }

    void setFreq(float f)
    {        
        mOsc.freq(f);
    }

    float process() override
    {
        return mOsc();
    }

private:
    gam::Sine<double> mOsc;
};




