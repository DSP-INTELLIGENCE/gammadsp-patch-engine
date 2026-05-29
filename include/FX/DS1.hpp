// GDSP_DS1.hpp
#pragma once
#include <cmath>
#include <algorithm>

struct OnePoleLP {
    float a = 0.0f;   // pole coefficient
    float z = 0.0f;   // state

    void setCutHz(float hz, float sr) {
        float fc = std::max(1.0f, hz);
        float x = std::exp(-2.0f * 3.14159265358979323846f * fc / std::max(1.0f, sr));
        a = x;
    }
    void reset(float v = 0.f) { z = v; }

    float process(float x) {
        z = a * z + (1.f - a) * x;
        return z;
    }
};

struct OnePoleHP {
    float a = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    void setCutHz(float hz, float sr) {
        float fc = std::max(1.0f, hz);
        a = std::exp(-2.0f * 3.14159265358979323846f * fc / std::max(1.0f, sr));
    }
    void reset() { x1 = y1 = 0.f; }

    float process(float x) {
        float y = a * (y1 + x - x1);
        x1 = x;
        y1 = y;
        return y;
    }
};

// Abel/Yeh-style rational saturator family (robust, pedal-friendly)
static inline float abelSaturate(float x, float Vc, float n) {
    // y = x / (1 + |x/Vc|^n)^(1/n)
    // Vc sets knee; n sets hardness.
    float ax = std::fabs(x);
    float k  = std::max(1e-6f, Vc);
    float nn = std::max(0.5f, n);
    float t  = std::pow(ax / k, nn);
    float d  = std::pow(1.f + t, 1.f / nn);
    return x / d;
}

class DS1 {
public:
    explicit DS1(float sampleRate)
    : sr(sampleRate)
    {
        setDrive(0.5f);
        setTone(0.5f);
        setLevel(0.5f);
        // default filters
        inHP.setCutHz(70.f, sr);      // tighten lows
        toneLP.setCutHz(2000.f, sr);  // will be updated by tone
        toneHP.setCutHz(900.f, sr);   // will be updated by tone
    }

    void setDrive(float d01) { drive01 = std::clamp(d01, 0.f, 1.f); }
    void setTone (float t01) { tone01  = std::clamp(t01, 0.f, 1.f); }
    void setLevel(float l01) { level01 = std::clamp(l01, 0.f, 1.f); }

    void setSampleRate(float sampleRate) {
        sr = std::max(8000.f, sampleRate);
        inHP.setCutHz(70.f, sr);
        updateToneFilters_();
    }

    void reset() {
        inHP.reset();
        toneLP.reset();
        toneHP.reset();
    }

    float process(float x) {
        // --- knob mapping (pedal-feel) ---
        // Drive: 0..60 dB, audio taper
        float drive_dB = 60.f * (drive01 * drive01);
        float preGain  = std::pow(10.f, drive_dB / 20.f);

        // Level: -24..+6 dB
        float level_dB = -24.f + 30.f * level01;
        float outGain  = std::pow(10.f, level_dB / 20.f);

        // Update tone filters (cheap; you can also update per-block instead of per-sample)
        updateToneFilters_();

        // --- input conditioning ---
        float u = inHP.process(x);

        // --- clipping core ---
        float v = u * preGain;

        // DS-1-ish: fairly hard, but not a pure hard-clip
        // Vc: knee threshold in normalized units; lower = earlier clipping
        // n : hardness; higher = harder clip
        constexpr float Vc = 0.35f;
        constexpr float n  = 2.8f;
        float clipped = abelSaturate(v, Vc, n);

        // Optional “bite” post-emphasis pre-tone (comment in if you want more edge)
        // clipped = clipped * 1.1f;

        // --- tone stage: blend dark (LPF) vs bright (HPF-ish) ---
        float dark   = toneLP.process(clipped);
        float bright = toneHP.process(clipped); // HP path emphasizes bite

        float y = (1.f - tone01) * dark + tone01 * bright;

        // --- output ---
        return y * outGain;
    }

    void run(const float* in, float* out, size_t nSamp) {
        for (size_t i = 0; i < nSamp; ++i) out[i] = process(in[i]);
    }

private:
    void updateToneFilters_() {
        // DS-1 tone approximation:
        // as Tone increases: raise LP cutoff and also raise HP cutoff a bit,
        // making it brighter and thinner.
        float t = tone01;

        // dark path LPF: 1.2 kHz .. 10 kHz
        float lpHz = 1200.f * std::pow(10.f, (std::log10(10000.f/1200.f) * t));
        // bright path HPF: 400 Hz .. 2.2 kHz
        float hpHz = 400.f  * std::pow(10.f, (std::log10(2200.f/400.f) * t));

        toneLP.setCutHz(lpHz, sr);
        toneHP.setCutHz(hpHz, sr);
    }

    float sr = 44100.f;

    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    OnePoleHP inHP;
    OnePoleLP toneLP;
    OnePoleHP toneHP;
};
