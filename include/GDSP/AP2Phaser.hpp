#pragma once
#include <algorithm>
#include <vector>

#ifndef SWIG
#include "ModAllPass1.hpp"
#include "ModAllPass2.hpp"
#include "Allpass1Block.hpp"
#include "Allpass2Block.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"
#endif

class AP2Phaser : public Function
{
public:
    AP2Phaser(int stages = 4)
    : ap2(800.f, 200.f, stages),
      baseFreq(800.f),
      baseWidth(200.f),
      baseFB(0.4f),
      baseMix(0.5f),
      last(0.f)
    {
        setIM(1.f);
        setAM(1.f);
    }

    // ---------------- Base controls ----------------
    void setFreq(float f)   { baseFreq = (std::max(5.f, f)); }
    void setWidth(float w)  { baseWidth = std::max(1.f, w); }
    void setFB(float f)     { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setMix(float m)    { baseMix = std::clamp(m, 0.f, 1.f); }

    // ---------------- Modulation inputs ----------------
    void setFM(float v)   { fm.set(v); }
    void setWM(float v)   { wm.set(v); }
    void setFBM(float v)  { fbm.set(v); }
    void setMIXM(float v) { mixm.set(v); }

    // ---------------- Gain ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) override
    {
        // --- Modulation ---
        float freq  = fm.process(baseFreq());
        float width = wm.process(baseWidth());
        float fb    = fbm.process(baseFB());
        float mix   = mixm.process(baseMix());

        freq  = std::max(freq, 5.f);
        width = std::max(width, 1.f);
        fb    = std::clamp(fb, -0.99f, 0.99f);
        mix   = std::clamp(mix, 0.f, 1.f);

        // --- Feedback loop ---
        float x = im.process(input + last * fb);
        x = std::tanh(x * 1.3f);   // chew + stability

        // --- Phase network ---
        ap2.freq(freq);
        ap2.width(width);
        x = ap2.process(x);

        // --- Output ---
        float out = input * (1.f - mix) + x * mix;
        last = x;

        return am.process(out);
    }

    void reset()
    {
        ap2.reset();
        last = 0.f;
    }


    AllPass2Block ap2;

    Parameter baseFreq;
    Parameter baseWidth;
    Parameter baseFB;
    Parameter baseMix;
    float last;

    Modulator fm, wm, fbm, mixm;
    Modulator im, am;
};
