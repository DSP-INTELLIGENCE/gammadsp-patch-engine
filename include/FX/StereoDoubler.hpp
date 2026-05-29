#pragma once
#include "GDSP_ModDelay.hpp"

class ModStereoDoubler : public Function
{
public:
    ModStereoDoubler()
    {
        // classic double tracking values
        L.setDelay(0.016f);
        R.setDelay(0.021f);

        L.setFM(0.0f);
        R.setFM(0.0f);

        L.setMM(1.0f);
        R.setMM(1.0f);

        setRate(0.18f);
        setDepth(0.0012f);
        setWidth(1.0f);
        setMix(0.75f);
    }

    void setRate(float r)   { m_lfoL.set(r); m_lfoR.set(r * 0.93f); }
    void setDepth(float d)  { m_depth = d; }
    void setWidth(float w)  { m_width = std::clamp(w,0.f,1.f); }
    void setMix(float m)    { m_mix = std::clamp(m,0.f,1.f); }

    std::pair<float,float> process(float input)
    {
        float driftL = m_lfoL.process() * m_depth;
        float driftR = m_lfoR.process() * m_depth;

        L.setDM(driftL);
        R.setDM(driftR);

        float l = L.update(input);
        float r = R.update(input);

        float mid  = (l + r) * 0.5f;
        float side = (l - r) * 0.5f * m_width;

        float outL = mid + side;
        float outR = mid - side;

        outL = input * (1.f - m_mix) + outL * m_mix;
        outR = input * (1.f - m_mix) + outR * m_mix;

        return { outL, outR };
    }

private:
    ModDelay L, R;
    Modulator m_lfoL, m_lfoR;

    float m_depth = 0.0012f;
    float m_width = 1.0f;
    float m_mix   = 0.75f;
};

