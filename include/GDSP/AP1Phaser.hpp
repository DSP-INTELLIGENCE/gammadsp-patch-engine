#pragma once
#include <algorithm>
#include <vector>

class AP1Phaser : public Function
{
public:
    AP1Phaser(int stages = 4)
    : filters(800.f, stages),
      baseFreq(800.f),
      baseFB(0.4f),
      baseMix(0.5f),
      last(0.f)
    {
        setIM(1.f);
        setAM(1.f);
    }

    // ---------------- Base controls ----------------
    void setFreq(float f)   { baseFreq.set(std::max(5.f, f)); }    
    void setFB(float f)     { baseFB.set(std::clamp(f, -0.99f, 0.99f)); }
    void setMix(float m)    { baseMix.set(std::clamp(m, 0.f, 1.f)); }

    // ---------------- Modulation inputs ----------------
    void setFM(float v)   { fm.set(v); }    
    void setFBM(float v)  { fbm.set(v); }
    void setMIXM(float v) { mixm.set(v); }

    // ---------------- Gain ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input) noexcept override 
    {
        // --- Modulation ---
        float freq = fm.process(baseFreq());
        float fb   = fbm.process(baseFB());
        float mix  = mixm.process(baseMix());

        freq = std::max(freq, 5.f);
        fb   = std::clamp(fb, -0.99f, 0.99f);
        mix  = std::clamp(mix, 0.f, 1.f);

        // --- Feedback loop ---
        float x = im.process(input) + last * fb;

        // soft saturation = chewy + safe
        x = std::tanh(x * 1.3f);

        // --- Phase network ---
        filters.freq(freq);
        x = filters.process(x);

        // --- Output ---
        float out = input * (1.f - mix) + x * mix;
        last = x;

        return am.process(out);
    }

    void reset()
    {
        filters.reset();
        last = 0.f;
    }

    AllPass1Block filters;

    Parameter baseFreq;
    Parameter baseFB;
    Parameter baseMix;
    float     last;

    Modulator fm, fbm, mixm;
    Modulator im, am;
};


