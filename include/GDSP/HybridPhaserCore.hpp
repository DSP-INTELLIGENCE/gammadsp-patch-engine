#pragma once
#include <algorithm>
#include <vector>
#include <cmath>
#include <random>

// ------------------------------------------------------------
// HybridPhaserCore
// Multi-stage all-pass phaser / dispersion core.
// - Cutoff is ALWAYS in Hz.
// - Modulation is additive in Hz (never multiplicative on cutoff).
// - Optional nonlinearity can be injected in feedback loop later.
// ------------------------------------------------------------
class HybridPhaserCore : public Function
{
public:
    enum StageType { AP1, AP2 };

    HybridPhaserCore(int stages = 6,
                     StageType type = AP2,
                     float centerHz = 900.0f,
                     float spreadHz = 250.0f,
                     float widthHz  = 120.0f)   // only used for AP2
    : mType(type),
      mBaseCenter(centerHz),
      mBaseSpread(spreadHz),
      mBaseWidth(widthHz)
    {
        setStages(stages);
        setMix(1.0f);
        setFeedback(0.0f);

        setFM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);

        setIM(1.0f);
        setAM(1.0f);
    }

    // ----------------- Topology -----------------

    void setStageType(StageType t)
    {
        if (t == mType) return;
        mType = t;
        // rebuild stages with same count
        setStages((int)mStagesCount);
    }

    StageType stageType() const { return mType; }

    void setStages(int n)
    {
        n = std::clamp(n, 1, 24);
        mStagesCount = (unsigned)n;

        // rebuild either AP1 or AP2 stages
        mAP1.clear();
        mAP2.clear();
        mRatios.clear();
        mWidthRatios.clear();

        buildRatios(n);

        if (mType == AP1)
        {
            mAP1.reserve(n);
            for (int i = 0; i < n; ++i)
                mAP1.emplace_back(std::max(1.0f, mBaseCenter));
        }
        else
        {
            mAP2.reserve(n);
            for (int i = 0; i < n; ++i)
                mAP2.emplace_back(std::max(1.0f, mBaseCenter), std::max(0.0f, mBaseWidth));
        }

        reset();
        updateStages(0.0f,0.0f);
    }

    int getStages() const { return (int)mStagesCount; }

    void reset()
    {
        last = 0.0f;
        if (mType == AP1)
        {
            for (auto& s : mAP1) s.reset();
        }
        else
        {
            for (auto& s : mAP2) s.reset();
        }
    }

    // ----------------- Base parameters -----------------

    // Center frequency in Hz
    void setCenterHz(float hz)
    {
        mBaseCenter = std::max(1.0f, hz);
    }

    // Spread in Hz (how far stages are distributed around the center)
    void setSpreadHz(float hz)
    {
        mBaseSpread = std::max(0.0f, hz);
    }

    // Width in Hz for AP2 stages
    void setWidthHz(float hz)
    {
        mBaseWidth = std::max(0.0f, hz);
    }

    // Stage detune shape 0..1 (how much of the ratio spread is used)
    void setDetune(float d)
    {
        mDetune = std::clamp(d, 0.0f, 1.0f);
    }

    // Wet/dry
    void setMix(float m) { mBaseMix = std::clamp(m, 0.0f, 1.0f); }

    // Feedback coefficient
    void setFeedback(float fb) { mBaseFB = std::clamp(fb, -0.99f, 0.99f); }

    float centerHz() const { return mBaseCenter; }
    float spreadHz() const { return mBaseSpread; }
    float widthHz()  const { return mBaseWidth; }
    float detune()   const { return mDetune; }
    float mix()      const { return mBaseMix; }
    float feedback() const { return mBaseFB; }

    // ----------------- Modulation (signals in [-1,1]) -----------------
    //
    // IMPORTANT:
    // - FM is an additive offset in Hz:
    //     center = baseCenter + depthHz * fmSignal
    //
    // - Spread modulation is also additive in Hz if you choose to use it later.
    // - Feedback and mix use your "base + base*signal" model for consistency.

    // Center frequency modulation signal
    void setFM(float v) { fm.set(v); }

    // Center modulation depth in Hz (NOT a multiplier)
    void setFMDepthHz(float hz) { mDepthCenterHz = std::max(0.0f, hz); }

    // Feedback modulation signal
    void setFBM(float v) { fbm.set(v); }

    // Mix modulation signal
    void setMIXM(float v) { mixm.set(v); }

    // Gain stages
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ----------------- DSP -----------------

    float process(float input) override
    {
        // --- read mod signals ---
        const float _fm   = fm.process();     // [-1,1]
        const float _fbm  = fbm.process();    // [-1,1]
        const float _mixm = mixm.process();   // [-1,1]
        const float _im   = im.process();
        const float _am   = am.process();

        // --- compute parameters ---
        // Center is absolute Hz + additive Hz modulation
        float center = mBaseCenter + (mDepthCenterHz * _fm);

        // Spread is absolute Hz
        float spread = mBaseSpread;

        // Feedback: base + base*signal (your style)
        float fb = mBaseFB + (mBaseFB * _fbm);
        fb = std::clamp(fb, -0.99f, 0.99f);

        // Mix: base + base*signal (your style)
        float mix = mBaseMix + (mBaseMix * _mixm);
        mix = std::clamp(mix, 0.0f, 1.0f);

        // Safety clamp for center
        const float sr = (float)gam::sampleRate();
        center = std::clamp(center, 1.0f, 0.45f * sr);

        // Update per-sample stage freqs
        updateStages(center, spread);

        // --- feedback injection ---
        float x = (input * _im) + (last * fb);

        // --- all-pass chain ---
        float y = x;

        if (mType == AP1)
        {
            for (size_t i = 0; i < mAP1.size(); ++i)
                y = mAP1[i].process(y);
        }
        else
        {
            for (size_t i = 0; i < mAP2.size(); ++i)
                y = mAP2[i].process(y);
        }

        last = y;

        // --- wet/dry ---
        float out = input * (1.0f - mix) + y * mix;

        return out * _am;
    }

private:
    // Build ratio tables once (geometric-ish spread + slight jitter).
    void buildRatios(int stages)
    {
        stages = std::max(1, stages);

        // Phase-90 style geometric spread
        const float baseSpread = 0.07f;  // ±7%
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> tolF(0.99f, 1.01f);
        std::uniform_real_distribution<float> tolW(0.98f, 1.02f);

        mRatios.resize(stages);
        mWidthRatios.resize(stages);

        if (stages == 1)
        {
            mRatios[0] = 1.0f;
            mWidthRatios[0] = 1.0f;
            return;
        }

        for (int i = 0; i < stages; ++i)
        {
            float pos = (float(i) / (stages - 1)) - 0.5f;
            float spread = std::pow(1.0f + baseSpread, pos * 2.0f);

            mRatios[i]      = spread * tolF(rng);
            mWidthRatios[i] = tolW(rng);
        }
    }

    // Update all stage params from base center/spread/width.
    void updateStages(float centerHz, float spreadHz)
    {
        const float nyq = 0.45f * (float)gam::sampleRate();

        // distribute around center using ratio table blended by detune
        // and add an absolute spread offset that grows with stage position.
        const int N = (int)mStagesCount;
        if (N <= 0) return;

        for (int i = 0; i < N; ++i)
        {
            float t = (N == 1) ? 0.0f : (float(i) / float(N - 1));
            float pos = t - 0.5f; // -0.5..+0.5

            // ratio detune blend
            float ratio = 1.0f + mDetune * (mRatios[i] - 1.0f);

            // absolute spread in Hz (linear around center)
            float hzOffset = pos * 2.0f * spreadHz;

            float f = (centerHz * ratio) + hzOffset;
            f = std::clamp(f, 1.0f, nyq);

            if (mType == AP1)
            {
                // AllPass1Block only has freq()
                mAP1[i].freq(f);
            }
            else
            {
                // AllPass2Block has freq() + width()
                float wr = 1.0f + mDetune * (mWidthRatios[i] - 1.0f);
                float w = std::clamp(mBaseWidth * wr, 0.0f, nyq);

                mAP2[i].freq(f);
                mAP2[i].width(w);
            }
        }
    }

private:
    StageType mType = AP2;

    unsigned mStagesCount = 0;

    // Stage arrays (only one is used depending on type)
    std::vector<AllPass1> mAP1;  // if you want blocks, swap to AllPass1Block
    std::vector<AllPass2> mAP2;  // swap to AllPass2Block if desired

    // Detune/ratio tables
    std::vector<float> mRatios;
    std::vector<float> mWidthRatios;

    // Base params
    float mBaseCenter = 900.0f;
    float mBaseSpread = 250.0f;
    float mBaseWidth  = 120.0f;
    float mDetune     = 0.8f;

    float mBaseFB  = 0.0f;
    float mBaseMix = 1.0f;

    // Mod depth (Hz)
    float mDepthCenterHz = 600.0f;

    // Modulators
    Modulator fm;     // center FM signal
    Modulator fbm;    // feedback mod signal
    Modulator mixm;   // mix mod signal
    Modulator im;     // input gain
    Modulator am;     // output gain

    float last = 0.0f;
};
