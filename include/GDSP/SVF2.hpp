#pragma once
#include "SVF.hpp"

class SVF2: public Function
{
public:
    SVF2() = default;

    void setSampleRate(float fs);
    void reset();

    // Paper params:
    // r >= 0  (damping, Moog=1.0, CAT≈1.064)
    // kHat in [0,1) (normalized global feedback / resonance)
    // cutoffHz in Hz
    void setParams(float r, float kHat, float cutoffHz);

    // --- Core tone controls ---
    void setDrive(float amount);                 // 0..2+
    void enableGainCompensation(bool enabled);   // Eq. 26 inverse gain

    // --- Analog behavior layers (optional) ---
    void setAnalogFeel(float amount);            // 0..2 (asymmetry + thermal drift strength)
    void setCutoffSaturation(float amount);      // 0..2 (more dirt at low cutoff)
    void enablePitchStabilizer(bool enabled);
    void setPitchStabilizerAmount(float amount); // 0..1 (how strongly it corrects)
    void setPitchStabilizerThreshold(float kHatThreshold); // e.g. 0.85

    // Processing (stereo: ch = 0 or 1)
    float process(float x) override { return processChannel(x,0); }
    float processChannel(float x, int ch);

private:
    // internal
    void updateSVFCoeffsIfNeeded(float effectiveCutoffHz);

    // cheap PRNG for drift (no <random>)
    inline float rand01()
    {
        // xorshift32
        uint32_t x = rngState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState = x;
        // convert to [0,1)
        return (x & 0x00FFFFFFu) * (1.0f / 16777216.0f);
    }

    static inline float clamp(float x, float lo, float hi)
    {
        return (x < lo) ? lo : (x > hi) ? hi : x;
    }

    static inline float fastAbs(float x) { return (x >= 0.0f) ? x : -x; }
    static inline float signf(float x)   { return (x >= 0.0f) ? 1.0f : -1.0f; }

    static inline float asymmetricSat(float x, float a, float b)
    {
        const float k = (x >= 0.0f) ? a : b;
        // stable, smooth, bounded
        return x / (1.0f + fastAbs(x) * k);
    }

private:
    // two cascaded 2-pole SVFs
    SVF s1, s2;

    float sampleRate = 44100.0f;

    // parameters
    float r = 1.0f;
    float kHat = 0.0f;
    float cutoffHz = 1000.0f;

    // core shaping
    float drive = 0.0f;
    bool  gainComp = true;

    // analog layers
    float analogFeel = 0.0f;
    float cutoffSatAmount = 0.0f;

    bool  pitchStabOn = true;
    float pitchStabAmount = 0.35f;     // 0..1 (correction strength)
    float pitchStabThreshold = 0.85f;  // only engage near self-oscillation

    // feedback memory (per channel)
    float yFb[2] = {0.0f, 0.0f};

    // thermal drift state (global; you can make per-channel if desired)
    float drift = 0.0f;
    float driftVel = 0.0f;
    uint32_t rngState = 0x12345678u;

    // pitch stabilization state (per channel)
    float lastOutPS[2] = {0.0f, 0.0f};
    int   samplesSinceCross[2] = {0, 0};
    float measuredHz[2] = {0.0f, 0.0f};
    float pitchCorrection = 1.0f;

    // coefficient update throttling
    int   coefUpdateCounter = 0;
    float lastEffectiveCutoff = -1.0f;
};
