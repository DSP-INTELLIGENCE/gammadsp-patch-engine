#pragma once
#include "ModDelay.hpp"

class DigitalDelay : public ModDelay
{
public:
    DigitalDelay(float maxDelay = 2.0f, float initDelay = 0.35f)
    : ModDelay(maxDelay, initDelay)    
    {
        // Lock all modulators into neutral state
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);
        setIM(1.0f);        
        setAM(1.0f);

        setIpolType(gam::ipl::CUBIC);

        setDelay(initDelay);
        setFeedback(0.45f);
        setMix(0.30f);
    }
        
    // ---------- DSP ----------

    float process(float input)
    {        
        float out = ModDelay::process(input);
        return out;
    }    
};
