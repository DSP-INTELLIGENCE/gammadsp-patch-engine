#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <Gamma/Filter.h>
#include "ModAllPass2.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"

class DispersiveMultiStagePhaser : public Function
{
public:
    DispersiveMultiStagePhaser(int stages = 8)
    : baseFreq(350.f),
      baseWidth(180.f),
      baseFB(0.6f),
      baseMix(0.7f),
      spread(0.08f)   // 8% per-stage spacing
    {
        setRate(0.2f);
        setDepth(0.85f);
        setEnvAmount(0.3f);

        setDispersion(0.7f);      // overall width intensity
        setWidthMotion(0.35f);    // how much LFO/env mod width
        setEnvToWidth(0.25f);     // extra “breathing” smear

        setIM(1.f);
        setAM(1.f);

        chain.reserve(std::max(1, stages));
        for(int i = 0; i < stages; ++i)
        {
            chain.emplace_back(baseFreq, baseWidth);
            stageMul.push_back(stageMultiplier(i, stages));
        }
    }

    // ---------- Musical Controls ----------
    void setRate(float r)        { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)       { depth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f)    { baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setMix(float m)         { baseMix = std::clamp(m, 0.f, 1.f); }
    void setEnvAmount(float e)   { envAmt = std::clamp(e, 0.f, 1.f); }

    // Dispersion controls
    void setBaseFreq(float f)    { baseFreq  = std::max(40.f, f); }
    void setBaseWidth(float w)   { baseWidth = std::max(1.f,  w); }

    // 0..1: scales how wide/“smeary” the all-pass stages are
    void setDispersion(float d)  { disp = std::clamp(d, 0.f, 1.f); }

    // 0..1: how much motion affects width (group delay)
    void setWidthMotion(float w) { widthMotion = std::clamp(w, 0.f, 1.f); }

    // 0..1: how much envelope pushes width
    void setEnvToWidth(float e)  { envToWidth = std::clamp(e, 0.f, 1.f); }

    // 0..~0.25 recommended: per-stage frequency spread amount
    void setSpread(float s)      { spread = std::clamp(s, 0.f, 0.25f); rebuildStageMultipliers(); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Optional: call if sample-rate changes and you want the envelope constant to remain consistent.
    void setEnvFollow(float alpha) { envAlpha = std::clamp(alpha, 0.001f, 0.2f); }

    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // Envelope follower (fast/medium; tweak via setEnvFollow)
        float level = std::fabs(x);
        env += envAlpha * (level - env);

        // LFO: your Modulator provides the modulation signal (assumed [-1,1] or similar)
        float lfo = rate.process();

        // Combined motion (for frequency sweep)
        float motion = lfo * depth + env * envAmt;

        // Exponential sweep around base (≈ ±3 octaves at motion=±1)
        float cutoff0 = baseFreq * std::pow(2.f, motion * 3.f);
        cutoff0 = std::clamp(cutoff0, 40.f, 0.45f * sr);

        // Width (dispersion / group delay) breathes with motion + envelope
        // Keep width positive and bounded.
        float wMotion = (lfo * widthMotion) + (env * envToWidth);
        float width0  = baseWidth * (1.f + disp * (0.75f + 0.75f * wMotion));
        width0 = std::clamp(width0, 1.f, 8000.f);

        // Feedback topology: before chain (classic)
        float in = x * im.process() + last * baseFB;

        // Phase / dispersion network
        for(size_t i = 0; i < chain.size(); ++i)
        {
            float fc = cutoff0 * stageMul[i];
            fc = std::clamp(fc, 40.f, 0.45f * sr);

            // Slightly vary width per stage for richer dispersion texture
            float wi = width0 * (1.f + 0.15f * disp * stageWidthSkew((int)i));
            wi = std::clamp(wi, 1.f, 8000.f);

            chain[i].setCutoff(fc);
            chain[i].setWidth(wi);
            in = chain[i].process(in);
        }

        last = in;

        float out = x * (1.f - baseMix) + in * baseMix;
        out *= am.process();
        return out;
    }

private:
    // Stage multiplier spreads frequencies across stages around 1.0 (log-ish)
    float stageMultiplier(int i, int N) const
    {
        if(N <= 1) return 1.f;
        float t = (float)i / (float)(N - 1);     // 0..1
        float centered = (t - 0.5f) * 2.f;       // -1..1
        // exponential spread feels best: stageMul in roughly [2^-spread, 2^spread]
        return std::pow(2.f, centered * spread);
    }

    // Deterministic stage skew in [-1,1] (no random needed; repeatable)
    float stageWidthSkew(int i) const
    {
        // cheap hash-ish sine
        return std::sin((float)i * 12.9898f + 78.233f);
    }

    void rebuildStageMultipliers()
    {
        stageMul.resize(chain.size());
        int N = (int)chain.size();
        for(int i = 0; i < N; ++i)
            stageMul[i] = stageMultiplier(i, N);
    }

private:
    std::vector<ModAllPass2> chain;
    std::vector<float> stageMul;

    float baseFreq  = 350.f;
    float baseWidth = 180.f;

    float baseFB = 0.6f;
    float baseMix = 0.7f;

    float depth  = 0.85f;
    float envAmt = 0.3f;

    float disp        = 0.7f;
    float widthMotion = 0.35f;
    float envToWidth  = 0.25f;

    float spread = 0.08f;     // stage frequency spread (log domain)
    float envAlpha = 0.05f;   // envelope follower speed

    Modulator rate;
    Modulator im, am;

    float env  = 0.f;
    float last = 0.f;
};
