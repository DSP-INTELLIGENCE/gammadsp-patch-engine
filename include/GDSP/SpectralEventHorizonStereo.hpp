#pragma once
#include <cmath>
#include <algorithm>

class SpectralEventHorizonStereo
{
public:
    SpectralEventHorizonStereo()
    {
        setFeedback(0.6f);
        setCrossFeedback(0.1f);
        setBaseShift(20.f);
        setWidth(80.f);
        setRate(0.04f);
        setTilt(0.5f);
        setMix(0.7f);
    }

    // ---------- Musical Controls ----------
    void setFeedback(float f)        { feedback = std::clamp(f, 0.f, 0.98f); }
    void setCrossFeedback(float f)   { xfb = std::clamp(f, 0.f, 0.5f); }
    void setBaseShift(float hz)      { baseShift = std::max(0.f, hz); }
    void setWidth(float hz)          { width = std::max(0.f, hz); }
    void setRate(float hz)           { rate.set(std::max(0.001f, hz)); }
    void setTilt(float t)            { tilt = std::clamp(t, 0.f, 1.f); }
    void setMix(float m)             { mix = std::clamp(m, 0.f, 1.f); }
    void setFreeze(bool f) { freeze = f; }
    bool isFrozen() const { return freeze; }

    void reset()
    {
        phase = 0.f;
        lastL = lastR = 0.f;
        filterL.reset();
        filterR.reset();
    }

    // ---------- DSP ----------
    StereoSample process(float x)
    {
        // --- master modulation phase ---
        if (!freeze)
        {
            float r = rate.process();
            phase += r / (float)gam::sampleRate();
            phase -= std::floor(phase);
        }

        // Quadrature phases
        float pL = phase;
        float pR = phase + 0.25f;
        pR -= std::floor(pR);

        float lfoL = std::sin(pL * 6.28318530718f);
        float lfoR = std::sin(pR * 6.28318530718f);

        float ctrlL = 0.5f * (1.f + lfoL);
        float ctrlR = 0.5f * (1.f + lfoR);

        // --- dynamic spectral shift (locked during freeze) ---
        float shiftL = baseShift + ctrlL * width;
        float shiftR = baseShift + ctrlR * width;

        float maxHz = 0.45f * (float)gam::sampleRate();
        shiftL = std::min(shiftL, maxHz);
        shiftR = std::min(shiftR, maxHz);

        shifterL.freq( shiftL);
        shifterR.freq(-shiftR);

        // --- feedback (slightly boosted when frozen) ---
        float fbGain = freeze ? std::min(0.98f, feedback + 0.15f) : feedback;

        float fbL = lastL * fbGain + lastR * xfb;
        float fbR = lastR * fbGain + lastL * xfb;

        fbL = fbL / (1.f + std::abs(fbL));
        fbR = fbR / (1.f + std::abs(fbR));

        // --- input injection (reduced when frozen) ---
        float inGain = freeze ? 0.05f : 1.0f;
        float inL = x * inGain + fbL;
        float inR = x * inGain + fbR;

        // --- spectral processing ---
        float yL = shifterL(inL);
        float yR = shifterR(inR);

        yL = filterL(yL);
        yR = filterR(yR);

        lastL = yL;
        lastR = yR;

        // --- equal-power wet/dry mix ---
        float a = mix * 1.57079632679f;
        float dG = std::cos(a);
        float wG = std::sin(a);

        StereoSample out;
        out.outL = x * dG + yL * wG;
        out.outR = x * dG + yR * wG;
        return out;
    }


    void run(const float* input, float* outL, float* outR, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            StereoSample s = process(input[i]);
            outL[i] = s.outL;
            outR[i] = s.outR;
        }
    }

    // ---------- Modulation ----------
    Modulator rate;

private:
    // ---------- Spectral Core ----------
    gam::FreqShift<double> shifterL;
    gam::FreqShift<double> shifterR;

    gam::Biquad<double> filterL { 1000.0, 0.707, gam::LOW_PASS };
    gam::Biquad<double> filterR { 1000.0, 0.707, gam::LOW_PASS };


    // ---------- Parameters ----------
    float feedback  = 0.6f;
    float xfb       = 0.1f;   // cross-feedback
    float baseShift = 20.f;
    float width     = 80.f;
    float tilt      = 0.5f;
    float mix       = 0.7f;
    bool freeze = false;

    // ---------- State ----------
    float phase = 0.f;
    float lastL = 0.f;
    float lastR = 0.f;
};
