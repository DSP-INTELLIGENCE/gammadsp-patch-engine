#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

#include "FrequencyShifter.hpp"


class AcousticLens : public Function {
public:
    AcousticLens()
    {
        for(int i=0;i<6;i++)
            warp.emplace_back(700.f + i * 300.f);
    }

    void setFocus(float f) { baseFocus = f; }     // 0..1
    void setDepth(float d) { baseDepth = d; }     // 0..1

    float process(float x) override
    {
        float focus = m_focus.process(baseFocus);
        float depth = m_depth.process(baseDepth);
        float y = x;
        if(p_in_tap) y = p_in_tap->process(y);
        for(auto& a : warp)
        {
            float f = 300.f + focus * 4000.f;        
            a.freq(f);
            y = a.process(y);
        }

        float refraction = (focus - 0.5f) * 40.f;
        refraction = m_refract.process(refraction);
        shifter.setShift(refraction);
        y = shifter.process(y);

        modFM.setDepth(depth);
        y = modFM.process(y);
        if(p_out_tap) y = p_out_tap->process(y);
        return m_am.process(y);
    }

    Modulator m_focus;
    Modulator m_depth;
    Modulator m_refract;
    Modulator m_am;
    Function  *p_in_tap = nullptr;
    Function  *p_out_tap = nullptr;

    float baseFocus=0.0f;
    float baseDepth=0.0f;

    FrequencyShifter shifter;
    ModFM modFM;

private:
    std::vector<AllPass1> warp;    
};
