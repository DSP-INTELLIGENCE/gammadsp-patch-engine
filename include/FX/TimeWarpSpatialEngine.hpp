#pragma once
#include <algorithm>
#include <cmath>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

// Your modules
#include "GroupDelayModulator.hpp"
#include "NonlinearDispersivePhaser.hpp"
#include "SpectralMotionEngine.hpp"

// -------------------------------------------------------------
// Time-Warp Spatial Engine
// - Chains:
//   1) GroupDelayModulator        (group-delay field / warp)
//   2) NonlinearDispersivePhaser  (nonlinear dispersion texture)
//   3) SpectralMotionEngine       (motion EQ + phase + dispersion hub, stereo-friendly)
// - One master motion clock (external CV) drives all stages coherently.
// -------------------------------------------------------------
class TimeWarpSpatialEngine : public Function
{
public:
    TimeWarpSpatialEngine()
    {
        setMix(0.85f);
        setWidth(0.85f);

        setRateHz(0.18f);
        setDepth(0.80f);
        setWarp(0.65f);
        setSmear(0.55f);
        setChaos(0.35f);

        setFeedback(0.35f);
        setDrive(1.4f);

        setPreAmount(0.70f);   // group-delay contribution
        setMidAmount(0.75f);   // nonlinear dispersive contribution
        setPostAmount(0.85f);  // spectral motion contribution

        setIM(1.0f);
        setAM(1.0f);

        // Sensible internal defaults (you can expose more if you want)
        pre.setMix(1.0f);
        mid.setMix(1.0f);
        post.setMix(1.0f);

        // keep stable unless user pushes
        pre.setFeedback(0.0f);
        mid.setFeedback(0.0f);
        post.setFeedback(0.0f);
    }

    // ---------- Global ----------
    void setMix(float v)   { mix = std::clamp(v, 0.f, 1.f); }
    void setWidth(float v) { width = std::clamp(v, 0.f, 1.f); } // stereo width inside post

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ---------- Master motion controls ----------
    // base rate in Hz, then add external CV via rateCV.set(...)
    void setRateHz(float hz) { baseRateHz = std::max(0.001f, hz); }

    // macro depth in [0..1], then add external CV via depthCV.set(...)
    void setDepth(float v) { baseDepth = std::clamp(v, 0.f, 1.f); }

    // Warp = how much group-delay field spreads & moves
    void setWarp(float v)  { warp = std::clamp(v, 0.f, 1.f); }

    // Smear = dispersion amount (post-echo smear / metallic space)
    void setSmear(float v) { smear = std::clamp(v, 0.f, 1.f); }

    // Chaos = nonlinear complexity (drive + extra fb distribution)
    void setChaos(float v) { chaos = std::clamp(v, 0.f, 1.f); }

    // Shared feedback and drive character
    void setFeedback(float v) { fb = std::clamp(v, -0.95f, 0.95f); }
    void setDrive(float v)    { drive = std::max(0.0f, v); }

    // Stage contributions (0=bypass that stage, 1=full)
    void setPreAmount(float v)  { amtPre  = std::clamp(v, 0.f, 1.f); }
    void setMidAmount(float v)  { amtMid  = std::clamp(v, 0.f, 1.f); }
    void setPostAmount(float v) { amtPost = std::clamp(v, 0.f, 1.f); }

    // ---------- External CV busses (match your Modulator semantics) ----------
    // Write to these from your AutonomousModEngine or elsewhere:
    Modulator rateCV;   // expected [-1..1], scales baseRateHz
    Modulator depthCV;  // expected [-1..1], scales baseDepth
    Modulator warpCV;   // expected [-1..1], scales warp
    Modulator smearCV;  // expected [-1..1], scales smear
    Modulator chaosCV;  // expected [-1..1], scales chaos

    // ---------------------------------------------------------
    // Mono process (folds stereo path)
    // ---------------------------------------------------------
    float process(float x) override
    {
        float yL, yR;
        processStereo(x, x, yL, yR);
        return 0.5f * (yL + yR);
    }

    // ---------------------------------------------------------
    // Stereo processing (recommended)
    // ---------------------------------------------------------
    void processStereo(float inL, float inR, float& outL, float& outR)
    {
        const float sr = (float)gam::sampleRate();

        // --- master CV application ---
        // Modulator outputs are already scaled by modAmount/modScale.
        const float rcv = rateCV.process();   // [-1..1] typical
        const float dcv = depthCV.process();
        const float wcv = warpCV.process();
        const float scv = smearCV.process();
        const float ccv = chaosCV.process();

        float rateHz = baseRateHz * (1.0f + 0.9f * rcv);
        rateHz = std::max(0.001f, rateHz);

        float depth = baseDepth * (1.0f + 0.9f * dcv);
        depth = std::clamp(depth, 0.f, 1.f);

        float warpAmt  = std::clamp(warp  * (1.0f + 0.9f * wcv), 0.f, 1.f);
        float smearAmt = std::clamp(smear * (1.0f + 0.9f * scv), 0.f, 1.f);
        float chaosAmt = std::clamp(chaos * (1.0f + 0.9f * ccv), 0.f, 1.f);

        // Derive coherent per-stage parameters from the same macros
        // (warp drives group-delay dispersion, smear drives dispersive network, chaos drives nonlinear drive + extra fb)
        const float fbPre  = fb * (0.35f + 0.55f * warpAmt);
        const float fbMid  = fb * (0.45f + 0.65f * smearAmt);
        const float fbPost = fb * (0.25f + 0.50f * chaosAmt);

        const float driveMid = drive * (1.0f + 2.0f * chaosAmt);

        // --- drive input (common) ---
        float xL = inL * im.process();
        float xR = inR * im.process();

        // =====================================================
        // 1) PRE: Group Delay Field (warp)
        // =====================================================
        pre.setRate(rateHz);
        pre.setDepth(depth);
        pre.setDispersion(warpAmt);
        pre.setFeedback(fbPre);
        pre.setMix(1.0f); // stage internal wet

        float preL = pre.process(xL);
        float preR = pre.process(xR);

        // stage blend (bypass weighting)
        float aL = lerp(xL, preL, amtPre);
        float aR = lerp(xR, preR, amtPre);

        // =====================================================
        // 2) MID: Nonlinear Dispersive Texture (smear + chaos)
        // =====================================================
        mid.setRate(rateHz * (0.75f + 0.60f * smearAmt));
        mid.setDepth(depth);
        mid.setDispersion(smearAmt);
        mid.setFeedback(fbMid);
        mid.setDrive(driveMid);
        mid.setMix(1.0f);

        float midL = mid.process(aL);
        float midR = mid.process(aR);

        float bL = lerp(aL, midL, amtMid);
        float bR = lerp(aR, midR, amtMid);

        // =====================================================
        // 3) POST: Spectral Motion Hub (stereo motion + width)
        // =====================================================
        post.setRate(rateHz * (0.85f + 0.40f * chaosAmt));
        post.setDepth(std::clamp(depth * (0.75f + 0.35f * warpAmt), 0.f, 1.f));
        post.setEnvAmount(std::clamp(0.15f + 0.55f * chaosAmt, 0.f, 1.f));
        post.setWidth(width);

        // share feedback/dispersion feel
        post.setFeedback(fbPost);
        post.setDispersion(std::clamp(0.25f + 0.70f * smearAmt, 0.f, 1.f));

        // spectral tone anchor (optional: tie to warp to feel “time warp”)
        post.setBaseCutoff(800.f * std::pow(2.f, (warpAmt - 0.5f) * 1.2f));
        post.setResonance(std::clamp(0.7f + 0.8f * chaosAmt, 0.1f, 1.8f));

        float postL, postR;
        post.processStereo(bL, bR, postL, postR);

        float cL = lerp(bL, postL, amtPost);
        float cR = lerp(bR, postR, amtPost);

        // =====================================================
        // Global mix (equal power)
        // =====================================================
        const float dryG = std::cos(mix * 1.5707963f);
        const float wetG = std::sin(mix * 1.5707963f);

        float yL = inL * dryG + cL * wetG;
        float yR = inR * dryG + cR * wetG;

        // Gentle global safety / glue (optional but recommended)
        yL = std::tanh(yL * (1.0f + 0.35f * chaosAmt));
        yR = std::tanh(yR * (1.0f + 0.35f * chaosAmt));

        const float g = am.process();
        outL = yL * g;
        outR = yR * g;
    }

private:
    static inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }

private:
    // Stages
    GroupDelayModulator       pre;
    NonlinearDispersivePhaser mid;
    SpectralMotionEngine      post;

    // Global params
    float mix   = 0.85f;
    float width = 0.85f;

    float baseRateHz = 0.18f;
    float baseDepth  = 0.80f;

    float warp  = 0.65f;
    float smear = 0.55f;
    float chaos = 0.35f;

    float fb    = 0.35f;
    float drive = 1.4f;

    float amtPre  = 0.70f;
    float amtMid  = 0.75f;
    float amtPost = 0.85f;

    // Modulators
    Modulator im, am;
};
