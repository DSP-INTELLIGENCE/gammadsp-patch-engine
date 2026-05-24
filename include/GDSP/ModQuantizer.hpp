#pragma once
#include <algorithm>
#include <cmath>
#include "Quantizer.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModQuantizer : public Function
{
public:
    ModQuantizer(float freq = 2000.f, float step = 0.f)
    : mQuant(freq, step),
      baseFreq(freq),
      baseStep(step)
    {
        setIM(1.0f);
        setAM(1.0f);
        setMix(1.0f);
        setFB(0.0f);
    }

    // ---------------- Base controls ----------------
    void setFreq(float f) { baseFreq = std::max(0.01f, f); }
    void setStep(float s) { baseStep = std::max(0.0f, s); }
    void setMix(float m)  { baseMix  = std::clamp(m, 0.0f, 1.0f); }
    void setFB(float f)   { baseFB   = std::clamp(f, -0.99f, 0.99f); }

    // ---------------- Modulation depths ----------------
    void setFM(float v)   { fm.set(v); }
    void setSM(float v)   { sm.set(v); }
    void setMIXM(float v) { mixm.set(v); }
    void setFBM(float v)  { fbm.set(v); }

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        float _fm   = fm.process();    // [-1,1]
        float _sm   = sm.process();    // [-1,1]
        float _mixm = mixm.process();  // [-1,1]
        float _fbm  = fbm.process();   // [-1,1]

        float freq = baseFreq + baseFreq * _fm;
        float step = baseStep + baseStep * _sm;
        float mix  = baseMix  + baseMix  * _mixm;
        float fb   = baseFB   + baseFB   * _fbm;

        freq = std::max(freq, 0.01f);
        step = std::max(step, 0.0f);
        mix  = std::clamp(mix, 0.0f, 1.0f);
        fb   = std::clamp(fb, -0.99f, 0.99f);

        mQuant.setFreq(freq);
        mQuant.setStep(step);

        float in = input * im.process() + last * fb;
        float wet = mQuant.process(in);

        float out = input * (1.0f - mix) + wet * mix;
        out *= am.process();

        last = wet;
        return out;
    }

private:
    Quantizer mQuant;

    float baseFreq;
    float baseStep;
    float baseMix = 1.0f;
    float baseFB  = 0.0f;

    Modulator fm;
    Modulator sm;
    Modulator mixm;
    Modulator fbm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};
