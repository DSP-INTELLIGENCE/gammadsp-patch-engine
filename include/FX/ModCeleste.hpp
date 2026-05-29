#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>
#include "Celeste.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModCeleste : public Function
{
public:
    ModCeleste(float delay = 0.04f,
               float amount = 0.004f,
               float freq = 0.6f)
    : baseDelay(delay),
      baseAmount(amount),
      baseFreq(freq)
    {
        setIM(1.0f);
        setAM(1.0f);
        setMix(0.5f);
    }

    // ---------- Base controls ----------
    void setDelay(float v)  { baseDelay = v; }
    void setAmount(float v) { baseAmount = v; }
    void setFreq(float v)   { baseFreq = v; }
    void setMix(float v)    { baseMix = std::clamp(v, 0.0f, 1.0f); }

    // ---------- Modulation depths ----------
    void setDM(float v)   { dm.set(v); }
    void setAMM(float v)  { amod.set(v); }
    void setFM(float v)   { fm.set(v); }
    void setMIXM(float v) { mixm.set(v); }

    // ---------- Gain stages ----------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float _dm   = dm.process();
        float _amod = amod.process();
        float _fm   = fm.process();
        float _mixm = mixm.process();

        float delay  = baseDelay  + baseDelay  * _dm;
        float amount = baseAmount + baseAmount * _amod;
        float freq   = baseFreq   + baseFreq   * _fm;
        float mix    = baseMix    + baseMix    * _mixm;

        delay  = std::max(0.0f, delay);
        amount = std::max(0.0f, amount);
        freq   = std::max(0.01f, freq);
        mix    = std::clamp(mix, 0.0f, 1.0f);

        mCeleste.delay(delay);
        mCeleste.amount(amount);
        mCeleste.freq(freq);

        float y = mCeleste(input * im.process());

        float out = input * (1.0f - mix) + y * mix;
        return out * am.process();
    }

private:
    gam::Celeste<double> mCeleste;

    float baseDelay;
    float baseAmount;
    float baseFreq;
    float baseMix = 0.5f;

    Modulator dm;     // delay modulation
    Modulator amod;   // amount modulation
    Modulator fm;     // frequency modulation
    Modulator mixm;   // wet/dry modulation
    Modulator im;
    Modulator am;
};


