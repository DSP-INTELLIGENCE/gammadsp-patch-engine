#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <algorithm>


class TVSVFCore: public Filter {
public:
    TVSVFCore(float sampleRate = 48000.0f) { setSampleRate(sampleRate); reset(); }

    // --- policy layer (front-end) ---
    void setSampleRate(float fs);
    void setCutoffHz(float hz);   // stored; used to compute g
    void setQ(float q);           // stored; used to compute R

    // Mix / mode selection (canonical SVF mix)
    void setMix(float c_hp, float c_bp, float c_lp);
    void setModeLP()    { setMix(0.0f, 0.0f, 1.0f); }
    void setModeHP()    { setMix(1.0f, 0.0f, 0.0f); }
    void setModeBP()    { setMix(0.0f, 1.0f, 0.0f); }
    void setModeNotch() { setMix(1.0f, 0.0f, 1.0f); }
    // "LP - HP" helper (name it explicitly to avoid EQ confusion)
    void setModeLpMinusHp() { setMix(-1.0f, 0.0f, 1.0f); }

    // --- state ---
    void reset();
    void setState(float s1, float s2) { s1_ = s1; s2_ = s2; }
    void getState(float& s1, float& s2) const { s1 = s1_; s2 = s2_; }

    // --- tick APIs ---
    float process(float x); // uses stored params (cutoff/Q → g/R), then SVF tick

    // "Core-only" tick if you want to bypass mapping (canonical coeff set)
    float process_core(float x, float g, float R, float c_hp, float c_bp, float c_lp);

    // --- block APIs ---
    // Constant cutoff/Q for whole block
    void run_const(const float* x, float* y, std::size_t n);

    // Per-sample modulation cutoff/Q arrays (audio-rate)
    void run_mod(const float* x, const float* cutoffHz, const float* q, float* y, std::size_t n);

private:
    // Stored front-end parameters (policy layer)
    float fs_ = 48000.0f;
    float cutoffHz_ = 1000.0f;
    float q_ = 0.707f;

    // Stored canonical coeffs (updated by updateCoefficientsFromParams)
    float g_ = 0.0f;
    float R_ = 0.5f / 0.707f;

    // Mix
    float c_hp_ = 0.0f;
    float c_bp_ = 0.0f;
    float c_lp_ = 1.0f;

    // SVF state
    float s1_ = 0.0f;
    float s2_ = 0.0f;

    // Helpers
    static inline bool isFinite(float v) { return std::isfinite(v); }
    static inline float clampf(float v, float lo, float hi) { return std::min(std::max(v, lo), hi); }

    void updateCoefficientsFromParams();        // cutoff/Q → g/R (policy)
    static inline float computeG(float fs, float cutoffHz);
    static inline float computeR(float q);

    // The canonical SVF tick (DSP core). This is the only place that updates s1_/s2_.
    float tickCore_(float x, float g, float R, float c_hp, float c_bp, float c_lp);
};
