#pragma once
#include <algorithm>
#include "AM.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModAM : public Function
{
public:
    ModAM(float depth = 1.0f)
    : mAM(depth)
    {
        setDepth(1.0f);
    }

    // -------- Modulation controls --------
    void setMM(float v)    { mm.set(v); }   // mod input
    void setDM(float d)    { dm.set(d); }   // depth mod
    void setDepth(float f) { mAM.setDepth(f); dm.set(f); }   

    float process(float carrier)
    {
        mAM.setDepth(dm.process());
        float y = mAM.process(carrier, mm.process());
        return y;
    }

private:
    AM mAM;    
    Modulator dm;
    Modulator mm;
};


