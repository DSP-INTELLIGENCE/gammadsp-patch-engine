#pragma once
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

class ModToneStack : public Function
{
public:
    ModToneStack()
    {
        setBass(0.f);
        setMid(0.f);
        setTreble(0.f);
        setMidFreq(800.f);
        setQ(0.8f);
        setMix(1.f);
        setFeedback(0.f);
        setIM(1.f);
        setAM(1.f);
    }

    // ---------- Base Controls ----------
    void setBass(float v)   { bassBase = std::clamp(v, -12.f, 12.f); }
    void setMid(float v)    { midBase  = std::clamp(v, -12.f, 12.f); }
    void setTreble(float v) { trebBase = std::clamp(v, -12.f, 12.f); }

    void setMidFreq(float f){ midFreqBase = std::clamp(f, 100.f, 8000.f); }
    void setQ(float q)      { Q = std::clamp(q, 0.2f, 5.f); }

    void setMix(float m)    { mix = std::clamp(m, 0.f, 1.f); }
    void setFeedback(float f){ fb = std::clamp(f, -0.90f, 0.90f); }

    // ---------- Modulation ----------
    void setBassM(float v)   { bassM.set(v); }
    void setMidM(float v)    { midM.set(v); }
    void setTrebleM(float v) { trebM.set(v); }
    void setMidFreqM(float v){ midFreqM.set(v); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ---------- DSP ----------
    float process(float x) override
    {
        // feedback + IM
        float in = x * im.process() + fbLP * last;

        // smooth feedback coefficient
        fbLP += 0.002f * (fb - fbLP);

        // --- modulated parameters ---
        float bassDB  = bassBase  + bassBase  * bassM.process();
        float midDB   = midBase   + midBase   * midM.process();
        float trebDB  = trebBase  + trebBase  * trebM.process();

        float midFreq = midFreqBase + midFreqBase * midFreqM.process();
        midFreq = std::clamp(midFreq, 80.f, 0.45f * (float)gam::sampleRate());

        // --- configure filters ---
        low.type(gam::LOW_SHELF);   low.freq(150.f);     low.gain(bassDB);
        mid.type(gam::PEAKING);     mid.freq(midFreq);   mid.Q(Q); mid.gain(midDB);
        high.type(gam::HIGH_SHELF); high.freq(3000.f);   high.gain(trebDB);

        // --- EQ chain ---
        float y = low(in);
        y = mid(y);
        y = high(y);

        // --- wet/dry mix ---
        float out = x * (1.f - mix) + y * mix;

        // AM at the very end
        out *= am.process();

        // store for feedback
        last = zap(out);
        return last;
    }

private:
    gam::Biquad<> low, mid, high;

    // base params
    float bassBase = 0.f;
    float midBase  = 0.f;
    float trebBase = 0.f;
    float midFreqBase = 800.f;
    float Q = 0.8f;

    float mix = 1.f;
    float fb  = 0.f;
    float fbLP = 0.f;

    float last = 0.f;

    // modulators
    Modulator bassM, midM, trebM, midFreqM;
    Modulator im, am;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }
};
