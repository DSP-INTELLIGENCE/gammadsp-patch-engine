#pragma once
#include <algorithm>
#include <cmath>
#include "BBDDelayLine.hpp"

// Simple mode switch like the DMM toggle
enum class DMMModMode { OFF = 0, CHORUS = 1, VIBRATO = 2 };

class EHXMemoryMan
{
public:
    EHXMemoryMan(float sampleRate = 48000.0f, size_t bbdStages = 2048);

    // --- “Knobs” in normalized [0..1] domain ---
    void setDelayKnob(float norm);      // maps to ~30ms..700ms (tunable)
    void setFeedbackKnob(float norm);   // maps to safe musical regen
    void setBlendKnob(float norm);      // dry/wet
    void setRateKnob(float norm);       // maps to ~0.05..8 Hz (tunable)
    void setDepthKnob(float norm);      // maps to 0..max clock mod depth
    void setLevelKnob(float norm);      // output gain

    void setModMode(DMMModMode mode);

    // --- Optional “advanced / internal trims” ---
    void setToneTrim(float norm);       // 0..1 (darker->brighter)
    void setDriveTrim(float norm);      // 0..1
    void setAgeTrim(float norm);        // 0..1

    // --- DSP ---
    float process(float x);
    void reset();

    // --- Helpers ---
    void setSampleRate(float sr); // if you need to support SR changes

private:
    // knob curves / mapping
    static float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

    static float expMap(float n, float minVal, float maxVal, float k = 3.0f);
    static float powMap(float n, float minVal, float maxVal, float p = 2.0f);
    static float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }

    void pushParamsToEngine();

private:
    // DSP engine
    BBDDelayLine mBBD;

    // State
    float mSR = 48000.0f;
    DMMModMode mMode = DMMModMode::CHORUS;

    // Normalized knobs
    float kDelay = 0.5f;
    float kFeedback = 0.5f;
    float kBlend = 0.5f;
    float kRate = 0.35f;
    float kDepth = 0.25f;
    float kLevel = 0.6f;

    // Trims
    float tTone = 0.55f;
    float tDrive = 0.35f;
    float tAge = 0.35f;

    // Cached mapped params
    float pDelaySec = 0.35f;
    float pFeedback = 0.45f;
    float pBlend = 0.5f;
    float pRateHz = 0.25f;
    float pDepth = 0.002f;   // clock modulation depth (fractional)
    float pLevel = 1.0f;

    // “DMM-ish” ranges (tune these by ear / reference)
    float mMinDelaySec = 0.03f;   // 30 ms
    float mMaxDelaySec = 0.70f;   // 700 ms (some units go longer; your stages/clock decide)

    float mMinRateHz = 0.05f;
    float mMaxRateHz = 8.0f;

    float mMaxDepthChorus = 0.0040f; // gentle
    float mMaxDepthVibrato = 0.0100f; // stronger

    float mMaxFeedback = 0.985f; // allows self-osc but prevents numeric runaway
};
