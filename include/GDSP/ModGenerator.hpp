#pragma once
#include "Engine.hpp"
#include "Parameters.hpp"

struct ModGenerator : public Generator
{
    virtual float process() = 0;

    void setFM(float f)    { fm.set(f); }
    void setPM(float f)    { pm.set(f); }
    void setAM(float f)    { am.set(f); }
    void setRatio(float f){ ratio.set(f); }
    void setIndex(float f){ index.set(f); }

    float baseFreq = 440.f;   // 🔑 the real carrier frequency
    Modulator fm, pm, am, ratio, index;

protected:
    
    template<class Osc>
    float update(Osc& osc)
    {
        float _fm    = fm.process();
        float _pm    = pm.process();
        float _am    = am.process();
        float r      = ratio.process();
        float I      = index.process();

        float fc   = baseFreq;        // stable carrier
        float fmHz = fc * r;
        float df   = _fm * fmHz * I;

        osc.phaseAdd(_pm);
        osc.freq(fc + df);
        float y = osc();
        osc.phaseAdd(-_pm);

        return y * _am;
    }
};

