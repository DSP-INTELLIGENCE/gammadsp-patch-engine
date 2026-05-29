#pragma once
#include <algorithm>
#include "Engine.hpp"
#include "Parameters.hpp"
#include <Gamma/Delay.h>

class ModFM : public Function
{
public:
    ModFM(float maxDelay = 0.05f, float baseDelay = 0.005f)
    : delay(maxDelay), baseDelay(baseDelay)
    {
        setDepth(0.0f);
        setFB(0.0f);
        setIM(1.0f);
        setAM(1.0f);
    }

    // -------- Controls --------
    void setBaseDelay(float d) { baseDelay = std::max(d, 0.0f); }
    void setDepth(float d)     { baseDepth = std::max(d, 0.0f); }
    void setFB(float f)        { baseFb = std::clamp(f, 0.0f, 0.99f); }
    

    void setMM(float v) { mm.set(v); }   // modulation signal
    void setIM(float v) { im.set(v); }   // input gain
    void setAM(float v) { am.set(v); }   // output gain

    float process(float input)
    {
        float m  = mm.process(baseMod);           // [-1,1]
        float in = im.process(input);
        if(p_in_tap) in = p_in_tap->process(in);
        
        float delayTime = m_delay.process(m_baseDelay) + m_depth.process(mDepth) * m;
        delayTime = std::clamp(delayTime, 0.0f, delay.maxDelay());

        delay.delay(delayTime);

        float y = delay(in + m_fb.process(fb) * last);
        last = y;
        if(p_out_tap) y = p_out_tap->process(y);
        return am.process(y);
    }

    float baseDelay = 0.0f;    
    float baseDepth = 0.0f;
    float baseFb    = 0.0f;
    float baseMod   = 0.0f;
    Modulator mm;
    Modulator im;
    Modulator am;
    Modulator m_delay;
    Modulator m_depth;
    Modulator m_fb;
    Function  *p_in_tap = nullptr;
    Function  *p_out_tap = nullptr;

private:
    gam::Delay<float> delay;
    float last = 0.0f;
};
