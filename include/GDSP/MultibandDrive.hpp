#pragma once
#include <algorithm>
#include <cmath>

class MultibandDrive : public Function
{
public:
    MultibandDrive()
    {
        setDrive(0.5f);
        setLowDrive(0.4f);
        setMidDrive(0.6f);
        setHighDrive(0.3f);

        setCrossLow(200.f);
        setCrossHigh(3500.f);
        setMix(1.f);
    }

    // ---------------- Controls ----------------
    void setDrive(float v)     { driveBase = clamp01(v); }
    void setLowDrive(float v)  { lowBase   = clamp01(v); }
    void setMidDrive(float v)  { midBase   = clamp01(v); }
    void setHighDrive(float v) { highBase  = clamp01(v); }

    void setCrossLow(float hz)  { crossLow = hz; }
    void setCrossHigh(float hz) { crossHigh = hz; }

    void setMix(float v) { mix = clamp01(v); }

    void reset()
    {
        lpLow.reset();
        hpHigh.reset();
        lowSat = {};
        midSat = {};
        highSat = {};
        comp.reset();
        dc_x1 = dc_y1 = 0.f;
    }

    float process(float x) override
    {
        const float sr = gam::sampleRate();

        // --- update crossovers ---
        lpLow.setLPCut(crossLow, sr);
        hpHigh.setHPCut(crossHigh, sr);

        // --- split bands ---
        float low  = lpLow.processLP(x);
        float high = hpHigh.processHP(x);
        float mid  = x - low - high;

        // --- global drive ---
        float d = 1.f + driveBase * 6.f;

        // --- per-band saturation ---
        low  = lowSat.process(low  * (1.f + lowBase  * 5.f) * d);
        mid  = midSat.process(mid  * (1.f + midBase  * 6.f) * d);
        high = highSat.process(high * (1.f + highBase * 4.f) * d);

        // --- recombine ---
        float y = low + mid + high;

        // --- loudness compensation ---
        y = comp.process(y);

        // --- wet/dry ---
        y = x * (1.f - mix) + y * mix;

        return dcBlock(y);
    }

private:
    // ---------------- Utilities ----------------
    inline float clamp01(float v) const
    {
        return std::clamp(v, 0.f, 1.f);
    }

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x;
        dc_y1 = y;
        return zap(y);
    }

private:
    // ---------------- Filters ----------------
    TPT1Pole lpLow;
    TPT1Pole hpHigh;

    // ---------------- Saturators ----------------
    Saturator lowSat;
    Saturator midSat;
    Saturator highSat;

    // ---------------- Loudness ----------------
    DriveCompander comp;

    // ---------------- Params ----------------
    float driveBase = 0.5f;
    float lowBase   = 0.4f;
    float midBase   = 0.6f;
    float highBase  = 0.3f;

    float crossLow  = 200.f;
    float crossHigh = 3500.f;

    float mix = 1.f;

    // ---------------- DC ----------------
    float dc_x1 = 0.f, dc_y1 = 0.f;
};
