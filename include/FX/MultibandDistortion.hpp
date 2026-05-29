#pragma once
#include <algorithm>
#include "LR4Crossover2.hpp"
#include "OversampledWaveShaper.hpp"

// Simple dry/wet helper
inline float mix(float dry, float wet, float amt) {
    return dry + amt * (wet - dry);
}

class MultibandDistortion
{
public:
    void prepare()
    {
        splitLM.prepare();
        splitMH.prepare();

        // Default crossovers
        setCrossovers(250.f, 2500.f);

        // Attach shapers (you set these from outside)
        osLow.setShaper(&lowShaper);
        osMid.setShaper(&midShaper);
        osHigh.setShaper(&highShaper);
    }

    void reset()
    {
        splitLM.reset();
        splitMH.reset();
    }

    // ---- Crossover controls ----
    void setCrossovers(float fLowMid, float fMidHigh)
    {
        float fs = (float)gam::sampleRate();
        fLowMid  = std::clamp(fLowMid,  40.f, 0.40f * fs);
        fMidHigh = std::clamp(fMidHigh, fLowMid * 1.5f, 0.45f * fs);

        splitLM.setFreq(fLowMid);
        splitMH.setFreq(fMidHigh);
    }

    // ---- Per-band drive / mix / level ----
    void setLow(float drive, float mixAmt, float level)
    { lowDrive=drive; lowMix=mixAmt; lowLevel=level; }

    void setMid(float drive, float mixAmt, float level)
    { midDrive=drive; midMix=mixAmt; midLevel=level; }

    void setHigh(float drive, float mixAmt, float level)
    { highDrive=drive; highMix=mixAmt; highLevel=level; }

    // If you want CPU mode:
    void setOversampleLow(bool v)  { osLowEnabled  = v; }
    void setOversampleMid(bool v)  { osMidEnabled  = v; }
    void setOversampleHigh(bool v) { osHighEnabled = v; }

    // Attach transfer functions externally (recommended),
    // or set modes on your design shaper and rebuild LUTs outside.
    LUTWaveShaper lowShaper, midShaper, highShaper;

    inline float process(float x)
    {
        // ---- Split into 3 bands ----
        float low, rest;
        splitLM.process(x, low, rest);

        float mid, high;
        splitMH.process(rest, mid, high);

        // ---- Distort per band ----
        float lowWet  = processBand(low,  lowShaper,  osLow,  lowDrive,  osLowEnabled);
        float midWet  = processBand(mid,  midShaper,  osMid,  midDrive,  osMidEnabled);
        float highWet = processBand(high, highShaper, osHigh, highDrive, osHighEnabled);

        // ---- Blend dry/wet per band ----
        low  = mix(low,  lowWet,  lowMix)  * lowLevel;
        mid  = mix(mid,  midWet,  midMix)  * midLevel;
        high = mix(high, highWet, highMix) * highLevel;

        // ---- Recombine (LR4 sums cleanly) ----
        return low + mid + high;
    }

private:
    inline float processBand(float x, LUTWaveShaper& sh, OversampledWaveShaper& os,
                             float drive, bool osEnabled)
    {
        sh.setDrive(drive);

        if(osEnabled) return os.process(x);
        return sh.process(x);
    }

    LR4Crossover2 splitLM;
    LR4Crossover2 splitMH;

    OversampledWaveShaper osLow, osMid, osHigh;
    bool osLowEnabled = true, osMidEnabled = true, osHighEnabled = true;

    float lowDrive  = 1.0f, midDrive  = 1.0f, highDrive  = 1.0f;
    float lowMix    = 1.0f, midMix    = 1.0f, highMix    = 1.0f;
    float lowLevel  = 1.0f, midLevel  = 1.0f, highLevel  = 1.0f;
};
