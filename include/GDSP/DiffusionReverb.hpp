#pragma once

#include <algorithm>
#include <cmath>

#include "Engine.hpp"
#include "Parameters.hpp"
#include "Allpass1Block.hpp"
#include "Delay.hpp"
#include "OnePole.hpp"

class DiffusionReverb 
{
public:
    DiffusionReverb()
    : mPreDelay(0.2f, 0.02f),

      mInDiff1(900.0f, 4),
      mInDiff2(1400.0f, 4),
      mInDiff3(2100.0f, 4),

      // Max 1s per tank line for safety, init with base times
      mT1(1.0f, 0.0297f),
      mT2(1.0f, 0.0371f),
      mT3(1.0f, 0.0411f),
      mT4(1.0f, 0.0437f),

      mDamp1(9000.0f, gam::LOW_PASS),
      mDamp2(9000.0f, gam::LOW_PASS),
      mDamp3(9000.0f, gam::LOW_PASS),
      mDamp4(9000.0f, gam::LOW_PASS),

      mDiff1(1200.0f, 4),
      mDiff2(1800.0f, 4),
      mDiff3(2400.0f, 4),
      mDiff4(3000.0f, 4)
    {
        // ER defaults
        mER1 = 0.011f; mERG1 = 0.55f;
        mER2 = 0.017f; mERG2 = 0.40f;
        mER3 = 0.029f; mERG3 = 0.32f;
        mER4 = 0.043f; mERG4 = 0.25f;
        mER5 = 0.061f; mERG5 = 0.20f;
        mER6 = 0.073f; mERG6 = 0.16f;

        // Parameter init (avoid UB)
        mSize      = 0.6f;
        mDecay     = 0.55f;
        mDamping   = 0.45f;
        mDiffusion = 0.7f;
        mMix       = 0.35f;
        mFeedback  = 0.75f;

        setPredelay(20.0f);
        setSize(mSize);
        setDecay(mDecay);
        setDamping(mDamping);
        setDiffusion(mDiffusion);
        setMix(mMix);
    }

    // ---------- Controls ----------
    void setPredelay(float ms)
    {
        ms = std::clamp(ms, 0.0f, 200.0f);
        float sec = std::max(0.0001f, ms * 0.001f);
        mPreDelay.setDelay(sec);
    }

    void setSize(float s)
    {
        mSize = std::clamp(s, 0.0f, 1.0f);
        // 0..1 -> scale 0.65..2.0 (small room to larger hall)
        float scale = 0.65f + 1.35f * mSize;

        // base times (seconds)
        float t1 = std::clamp(0.0297f * scale, 0.005f, 0.25f);
        float t2 = std::clamp(0.0371f * scale, 0.005f, 0.25f);
        float t3 = std::clamp(0.0411f * scale, 0.005f, 0.25f);
        float t4 = std::clamp(0.0437f * scale, 0.005f, 0.25f);


        mT1.setDelay(t1);
        mT2.setDelay(t2);
        mT3.setDelay(t3);
        mT4.setDelay(t4);

        // If your Delay::read(ago) uses seconds, you may want ER scaling too:
        // (optional) keep ER proportional to room size
        // float erScale = 0.8f + 0.8f * mSize;
        // mER1 *= erScale; ... (but don't repeatedly scale in-place)
    }

    void setDecay(float norm)
    {
        // map 0..1 -> 0.4s .. 12s
        float rt = 0.4f + norm * norm * 11.6f;
        setRT60(rt);
    }

    void setDamping(float d)
    {
        mDamping = std::clamp(d, 0.0f, 1.0f);

        float nyq = 0.49f * gam::sampleRate();
        float cutoff = 14000.0f - 11800.0f * mDamping; // 14k -> 2.2k
        cutoff = std::clamp(cutoff, 200.0f, nyq);

        mDamp1.setFreq(cutoff);
        mDamp2.setFreq(cutoff);
        mDamp3.setFreq(cutoff);
        mDamp4.setFreq(cutoff);
    }

    void setDiffusion(float d)
    {
        mDiffusion = std::clamp(d, 0.0f, 1.0f);
        // Use detune as diffusion-spread proxy
        float det = 0.05f + 0.95f * mDiffusion;

        mInDiff1.setDetune(det);
        mInDiff2.setDetune(det);
        mInDiff3.setDetune(det);

        mDiff1.setDetune(det);
        mDiff2.setDetune(det);
        mDiff3.setDetune(det);
        mDiff4.setDetune(det);
    }

    void setMix(float m)
    {
        mMix = std::clamp(m, 0.0f, 1.0f);
    }

    void setWidth(float w)
    {
        mWidth = std::clamp(w, 0.0f, 1.0f);
    }

    void setRT60(float seconds)
    {
        mRT60 = std::clamp(seconds, 0.2f, 20.0f);   // sane musical range

        updateFeedbackGains();
    }

    // ---------- DSP ----------
    StereoSample process(float x)
    {
        // Predelay write/read
        mPreDelay.write(x);
        float pd = mPreDelay.read();

        // Early reflections (read(agoSeconds))
        float er =
            mPreDelay.read(mER1) * mERG1 +
            mPreDelay.read(mER2) * mERG2 +
            mPreDelay.read(mER3) * mERG3 +
            mPreDelay.read(mER4) * mERG4 +
            mPreDelay.read(mER5) * mERG5 +
            mPreDelay.read(mER6) * mERG6;

        // Injection signal
        float in = 0.75f * pd + 0.45f * er;

        // Input diffusion
        in = mInDiff1.process(in);
        in = mInDiff2.process(in);
        in = mInDiff3.process(in);

        // Read tank outputs
        float y1 = mT1.read();
        float y2 = mT2.read();
        float y3 = mT3.read();
        float y4 = mT4.read();

        // 4x4 Hadamard-ish mix (energy balanced)
        float v1 = 0.5f * ( y1 + y2 + y3 + y4 );
        float v2 = 0.5f * ( y1 - y2 + y3 - y4 );
        float v3 = 0.5f * ( y1 + y2 - y3 - y4 );
        float v4 = 0.5f * ( y1 - y2 - y3 + y4 );

        // Per-line damping + diffusion
        v1 = mDiff1.process(mDamp1.process(v1));
        v2 = mDiff2.process(mDamp2.process(v2));
        v3 = mDiff3.process(mDamp3.process(v3));
        v4 = mDiff4.process(mDamp4.process(v4));

        // Write back (injection + feedback)
        //mT1.write(in + v1 * mFeedback);
        //mT2.write(in + v2 * mFeedback);
        //mT3.write(in + v3 * mFeedback);
        //mT4.write(in + v4 * mFeedback);

        mT1.write(in + v1 * mG1);
        mT2.write(in + v2 * mG2);
        mT3.write(in + v3 * mG3);
        mT4.write(in + v4 * mG4);

        // --- Stereo projection ---
        float midL =  0.60f*y1 + 0.25f*y2 - 0.40f*y3 + 0.15f*y4;
        float midR =  0.15f*y1 - 0.40f*y2 + 0.25f*y3 + 0.60f*y4;

        // normalize energy
        const float norm = 0.85f;
        midL *= norm;
        midR *= norm;

        // width control (mid/side style)
        float mono = 0.5f * (midL + midR);
        float side = 0.5f * (midL - midR);

        side *= mWidth;

        float outL = mono + side;
        float outR = mono - side;

        // mix
        outL = x * (1.0f - mMix) + outL * mMix;
        outR = x * (1.0f - mMix) + outR * mMix;

        return StereoSample(outL,outR);

    }

    void updateFeedbackGains()
    {
        // current tank delays (seconds)
        float t1 = mT1.getDelayTime();
        float t2 = mT2.getDelayTime();
        float t3 = mT3.getDelayTime();
        float t4 = mT4.getDelayTime();

        auto gain = [&](float t)
        {
            return std::pow(10.0f, -3.0f * t / mRT60);
        };

        mG1 = gain(t1);
        mG2 = gain(t2);
        mG3 = gain(t3);
        mG4 = gain(t4);
    }

private:
    // Predelay
    Delay mPreDelay;

    // Early reflections taps (seconds) + gains
    float mER1, mER2, mER3, mER4, mER5, mER6;
    float mERG1, mERG2, mERG3, mERG4, mERG5, mERG6;

    // Input diffusion
    AllPass1Block mInDiff1;
    AllPass1Block mInDiff2;
    AllPass1Block mInDiff3;

    // Tank: delays + damping + diffusion
    Delay mT1, mT2, mT3, mT4;
    OnePole mDamp1, mDamp2, mDamp3, mDamp4;
    AllPass1Block mDiff1, mDiff2, mDiff3, mDiff4;

    // Parameters
    float mSize      = 0.6f;
    float mDecay     = 0.55f;
    float mDamping   = 0.45f;
    float mDiffusion = 0.7f;
    float mMix       = 0.35f;
    float mWidth = 0.7f;   // 0 = mono, 1 = maximum width
    float mFeedback  = 0.75f;
    float mRT60 = 3.5f;
    float mG1, mG2, mG3, mG4;

};
