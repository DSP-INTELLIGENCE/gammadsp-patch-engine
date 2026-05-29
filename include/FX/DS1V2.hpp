// GDSP_DS1_SaturationEngine.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "TOnePoleLP.hpp"
#include "TOnePoleHP.hpp"
#include "TBlockDC.hpp"

// ------------------------------------------------
// Enums (match DS1 v2 layout)
// ------------------------------------------------
enum DS1Mode {
    Stock,
    TurboI,
    TurboII,
    Keeley
};

enum SaturationType {
    AbelYeh,
    Diode,
    LED,
    MOSFET,
    JFET
};

// ------------------------------------------------
// Utility
// ------------------------------------------------
static inline float clampf(float x, float a, float b) {
    return std::max(a, std::min(b, x));
}

// ------------------------------------------------
// Abel/Yeh rational clip
// ------------------------------------------------
static inline float abelSaturate(float x, float Vc, float n) {
    float ax = std::fabs(x);
    float k  = std::max(1e-6f, Vc);
    float nn = std::max(0.5f, n);
    float t  = std::pow(ax / k, nn);
    float d  = std::pow(1.f + t, 1.f / nn);
    return x / d;
}

static inline float diodeAsymSaturate(
    float x, float Vp, float Vn, float np, float nn, float bias
) {
    float xb = x + bias;
    if (xb >= 0.f) {
        float t = std::pow(std::fabs(xb) / std::max(1e-6f, Vp), np);
        return xb / std::pow(1.f + t, 1.f / np);
    } else {
        float t = std::pow(std::fabs(xb) / std::max(1e-6f, Vn), nn);
        return xb / std::pow(1.f + t, 1.f / nn);
    }
}

// ------------------------------------------------
// LED model
// (same family as diode, just different params; function exists for clarity)
// ------------------------------------------------
static inline float ledAsymSaturate(
    float x, float Vp, float Vn, float np, float nn, float bias
) {
    return diodeAsymSaturate(x, Vp, Vn, np, nn, bias);
}

// ------------------------------------------------
// MOSFET model
// square-law-ish with even harmonics + safe output bounding
// ------------------------------------------------
static inline float mosfetClip(float x, float drive, float bias) {
    float xb = x + bias;

    // "tube-ish" core
    float y = std::tanh(drive * xb);

    // square-law enrichment (even harmonics / chew)
    // keep this term drive-aware so it doesn't explode at high preGain
    float k = 0.15f + 0.20f * clampf((drive - 1.0f) * 0.15f, 0.f, 1.f);
    float sq = (xb * xb * xb) * k;

    float out = y + sq;

    // final soft bound (prevents runaway with extreme input)
    return std::tanh(out);
}

// ------------------------------------------------
// JFET model
// triode-ish asym tanh + compressive shaping, bounded
// ------------------------------------------------
static inline float jfetClip(float x, float drive, float bias) {
    float xb = x + bias;

    float dPos = drive * 1.10f;
    float dNeg = drive * 0.90f;

    float y = (xb >= 0.f) ? std::tanh(dPos * xb) : std::tanh(dNeg * xb);

    // gentle compress / "sag" feel
    y *= (1.0f - 0.20f * std::fabs(y));

    return std::tanh(y);
}

// ------------------------------------------------
// Shared param type
// ------------------------------------------------
struct ClipParams {
    float Vp = 0.32f;
    float Vn = 0.46f;
    float np = 3.2f;
    float nn = 2.4f;
    float bias = -0.04f;
};

// -----------------------------
// SaturationEngine
// -----------------------------
struct SaturationEngine {
    SaturationType type = SaturationType::Diode;
    DS1Mode mode = DS1Mode::Stock;

    // Optional: drive-aware feel (set from outside if desired), 0..1
    // If disabled, keep drive01 < 0.
    float drive01 = -1.f;

    float process(float x) {
        switch (type) {
            case SaturationType::AbelYeh: return processAbelYeh(x);
            case SaturationType::LED:     return processLED(x);
            case SaturationType::MOSFET:  return processMOSFET(x);
            case SaturationType::JFET:    return processJFET(x);
            case SaturationType::Diode:
            default:                      return processDiode(x);
        }
    }

private:
    // ---------------- Mode tables ----------------
    ClipParams diodeParamsForMode_() const {
        switch (mode) {
            default:
            case DS1Mode::Stock:
                return { 0.32f, 0.46f, 3.2f, 2.4f, -0.04f };

            case DS1Mode::TurboI:
                return { 0.36f, 0.52f, 3.0f, 2.2f, -0.03f };

            case DS1Mode::TurboII:
                // "diode TurboII" = lifted diode feel
                return { 0.42f, 0.60f, 2.7f, 2.1f, -0.02f };

            case DS1Mode::Keeley:
                return { 0.30f, 0.50f, 3.4f, 2.2f, -0.06f };
        }
    }

    ClipParams ledParamsForMode_() const {
        switch (mode) {
            default:
            case DS1Mode::Stock:
                return { 0.55f, 0.70f, 2.4f, 2.0f, -0.02f };

            case DS1Mode::TurboI:
                return { 0.50f, 0.66f, 2.5f, 2.1f, -0.02f };

            case DS1Mode::TurboII:
                return { 0.60f, 0.78f, 2.3f, 1.9f, -0.015f };

            case DS1Mode::Keeley:
                return { 0.52f, 0.74f, 2.4f, 2.0f, -0.03f };
        }
    }

    // ---------------- Drive helpers ----------------
    float driveScaled_() const {
        if (drive01 < 0.f) return 1.0f;
        float d = std::clamp(drive01, 0.f, 1.f);
        // 1 .. 7 (musical range; tweak to taste)
        return 1.0f + 6.0f * d;
    }

    float biasScaled_(float baseBias) const {
        if (drive01 < 0.f) return baseBias;
        float d = std::clamp(drive01, 0.f, 1.f);
        // more drive -> slightly more negative bias -> more asym chew
        return baseBias - 0.04f * d;
    }

    float applyDriveBias_(float bias) const {
        return biasScaled_(bias);
    }

    // ---------------- Implementations ----------------
    float processAbelYeh(float x) {
        float Vc = 0.35f;
        float n  = 2.8f;

        switch (mode) {
            default:
            case DS1Mode::Stock:
                Vc = 0.35f; n = 2.8f; break;
            case DS1Mode::TurboI:
                Vc = 0.40f; n = 2.6f; break;
            case DS1Mode::TurboII:
                Vc = 0.50f; n = 2.3f; break;
            case DS1Mode::Keeley:
                Vc = 0.33f; n = 2.9f; break;
        }

        return abelSaturate(x, Vc, n);
    }

    float processDiode(float x) {
        ClipParams p = diodeParamsForMode_();
        p.bias = applyDriveBias_(p.bias);

        p.Vp = std::max(1e-6f, p.Vp);
        p.Vn = std::max(1e-6f, p.Vn);
        p.np = std::max(0.5f, p.np);
        p.nn = std::max(0.5f, p.nn);

        return diodeAsymSaturate(x, p.Vp, p.Vn, p.np, p.nn, p.bias);
    }

    float processLED(float x) {
        ClipParams p = ledParamsForMode_();
        p.bias = applyDriveBias_(p.bias);

        p.Vp = std::max(1e-6f, p.Vp);
        p.Vn = std::max(1e-6f, p.Vn);
        p.np = std::max(0.5f, p.np);
        p.nn = std::max(0.5f, p.nn);

        return ledAsymSaturate(x, p.Vp, p.Vn, p.np, p.nn, p.bias);
    }

    float processMOSFET(float x) {
        float d = driveScaled_();
        float bias = biasScaled_(-0.03f);

        // Mode affects chew vs openness
        if (mode == DS1Mode::TurboII) d *= 0.85f;
        if (mode == DS1Mode::Keeley)  d *= 1.15f;

        return mosfetClip(x, d, bias);
    }

    float processJFET(float x) {
        float d = driveScaled_();
        float bias = biasScaled_(-0.05f);

        // JFETs are softer and more asymmetric
        if (mode == DS1Mode::Stock)   d *= 0.90f;
        if (mode == DS1Mode::Keeley)  d *= 1.20f;

        return jfetClip(x, d, bias);
    }
};


enum ToneType {
    ClassicBlend
};
enum Quality {
    Pedal
};

struct DS1Config {
    // --- global ---
    float sampleRate = 44100.f;

    // --- knobs ---
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // --- modes ---
    SaturationType saturation = SaturationType::Diode;
    DS1Mode        mode       = DS1Mode::Stock;
    ToneType       toneType   = ToneType::ClassicBlend;
    Quality        quality    = Quality::Pedal;
};

struct DSPBlock {
    void setSampleRate(float) {}
    void reset() {}
};

struct InputStage : DSPBlock {
    TOnePoleHP<float> hp;

    void setSampleRate(float sr) {
        hp.freq(70.f);
    }

    float process(float x) {
        return hp.process(x);
    }

    void reset() { hp.reset(); }
};

struct GainStage {
    float drive01 = 0.5f;
    float level01 = 0.5f;

    float preGain  = 1.f;
    float outGain  = 1.f;

    void update() {
        float drive_dB = 60.f * (drive01 * drive01);
        preGain = std::pow(10.f, drive_dB / 20.f);

        float level_dB = -24.f + 30.f * level01;
        outGain = std::pow(10.f, level_dB / 20.f);
    }
};

struct DCBlock : DSPBlock {
    TOnePoleHP<float> hp;

    void setSampleRate(float sr) {
        hp.freq(25.f);
    }

    float process(float x) {
        return hp.process(x);
    }

    void reset() { hp.reset(); }
};

struct PostShape : DSPBlock {
    DS1Mode mode = DS1Mode::Stock;

    TOnePoleLP<float> fizzLP;
    TOnePoleLP<float> bassLP;

    void setSampleRate(float sr) {
        fizzLP.freq(7000.f);
        bassLP.freq(150.f);
    }

    float process(float x) {
        if (mode == DS1Mode::TurboII || mode == DS1Mode::Keeley) {
            float lows = bassLP.process(x);
            x = 0.7f * x + 0.3f * lows;
        }
        if (mode == DS1Mode::Keeley) {
            x = fizzLP.process(x);
        }
        return x;
    }

    void reset() {
        fizzLP.reset();
        bassLP.reset();
    }
};

struct ToneNetwork : DSPBlock {
    ToneType type = ToneType::ClassicBlend;
    float tone01 = 0.5f;

    // classic DS-1
    TOnePoleLP<float> lp;
    TOnePoleHP<float> hp;

    void setSampleRate(float sr) {
        update(sr);
    }

    void setTone(float t01, float sr) {
        tone01 = t01;
        update(sr);
    }

    float process(float x) {
        switch (type) {
            case ToneType::ClassicBlend:
            default: {
                float dark   = lp.process(x);
                float bright = hp.process(x);
                return (1.f - tone01) * dark + tone01 * bright;
            }
        }
    }

    void reset() {
        lp.reset();
        hp.reset();
    }

private:
    void update(float sr) {
        float t = tone01;
        float lpHz = 1200.f * std::pow(10.f, std::log10(10000.f / 1200.f) * t);
        float hpHz = 400.f  * std::pow(10.f, std::log10(2200.f / 400.f)  * t);
        lp.freq(lpHz);
        hp.freq(hpHz);
    }
};

struct QualityManager {
    Quality quality = Quality::Pedal;

    template<typename Fn>
    float process(float x, Fn&& nonlinear) {
        if (quality == Quality::Pedal) {
            return nonlinear(x);
        }
        // 2x / 4x placeholder
        return nonlinear(x);
    }
};

class DS1v2 {
public:
    void setConfig(const DS1Config& c) {
        cfg = c;

        input.setSampleRate(cfg.sampleRate);
        dc.sampleRate(cfg.sampleRate);
        post.setSampleRate(cfg.sampleRate);
        tone.setSampleRate(cfg.sampleRate);

        gain.drive01 = cfg.drive01;
        gain.level01 = cfg.level01;
        gain.update();

        sat.type = cfg.saturation;
        sat.mode = cfg.mode;
        post.mode = cfg.mode;

        tone.type = cfg.toneType;
        tone.setTone(cfg.tone01, cfg.sampleRate);

        quality.quality = cfg.quality;
    }

    float process(float x) {
        x = input.process(x);
        x *= gain.preGain;

        x = quality.process(x, [&](float s) {
            return sat.process(s);
        });

        x = dc.process(x);
        x = post.process(x);
        x = tone.process(x);

        return x * gain.outGain;
    }

    void reset() {
        input.reset();
        dc.reset();
        post.reset();
        tone.reset();
    }

private:
    DS1Config cfg;

    InputStage     input;
    GainStage      gain;
    SaturationEngine sat;
    TBlockDC<float> dc;
    PostShape      post;
    ToneNetwork    tone;
    QualityManager quality;
};
