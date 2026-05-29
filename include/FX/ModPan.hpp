#pragma once
#include <algorithm>
#include "Pan.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class ModPan : public Function
{
public:
    ModPan(float pos = 0.0f)
    : basePos(pos)
    {
        setIM(1.0f);
        setAM(1.0f);
        setFB(0.0f);
        setMix(1.0f);
    }

    // -------- Base controls --------
    void setPos(float p) { basePos = std::clamp(p, -1.0f, 1.0f); }
    void setMix(float m) { baseMix = std::clamp(m, 0.0f, 1.0f); }
    void setFB(float f)  { baseFB  = std::clamp(f, -0.99f, 0.99f); }

    // -------- Modulation depths --------
    void setPM(float v)   { pm.set(v); }
    void setMIXM(float v) { mixm.set(v); }
    void setFBM(float v)  { fbm.set(v); }

    // -------- Gain stages --------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // -------- Processing --------
    void process(float input, float& left, float& right)
    {
        float _pm   = pm.process();
        float _mixm = mixm.process();
        float _fbm  = fbm.process();

        float pos = basePos + basePos * _pm;
        pos = std::clamp(pos, -1.0f, 1.0f);

        float mix = baseMix + baseMix * _mixm;
        mix = std::clamp(mix, 0.0f, 1.0f);

        float fb = baseFB + baseFB * _fbm;
        fb = std::clamp(fb, -0.99f, 0.99f);

        float in = input * im.process() + last * fb;

        float l, r;
        pan.setPos(pos);
        pan.process(in, l, r);

        left  = input * (1.0f - mix) + l * mix;
        right = input * (1.0f - mix) + r * mix;

        float g = am.process();
        left  *= g;
        right *= g;

        last = 0.5f * (l + r);
    }

private:
    Pan pan;

    float basePos = 0.0f;
    float baseMix = 1.0f;
    float baseFB  = 0.0f;

    Modulator pm;
    Modulator mixm;
    Modulator fbm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};
