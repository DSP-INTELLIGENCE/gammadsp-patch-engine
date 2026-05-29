#pragma once
#include <algorithm>
#include <cmath>

#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

// Your existing modules
#include "ModMotionFilter.hpp"
#include "PhaseShifter.hpp"
#include "DispersivePhaser.hpp"

// -------------------------------------------------------------
// Spectral Motion Engine
// - Combines MotionFilter + PhaseShifter + DispersivePhaser
// - Mid/Side processing with stereo motion + width control
// - Macro motion clock drives rate/depth and can be "pushed" by envelope
// -------------------------------------------------------------
class SpectralMotionEngine : public Function
{
public:
    SpectralMotionEngine()
    {
        setMix(0.85f);
        setWidth(0.75f);

        setRate(0.22f);
        setDepth(0.80f);
        setEnvAmount(0.35f);
        setEnvTime(0.05f);

        setMotionAmount(0.55f);
        setPhaseAmount(0.55f);
        setDispersionAmount(0.45f);

        setFeedback(0.35f);
        setDispersion(0.55f);

        setBaseCutoff(900.f);
        setResonance(0.8f);

        setIM(1.0f);
        setAM(1.0f);

        // Sub-module defaults
        motion.setMix(1.0f);
        motion.setBaseCutoff(baseCutoff);
        motion.setResonance(baseRes);

        phaser.setMix(1.0f);
        phaser.setFeedback(0.0f);

        disper.setMix(1.0f);
        disper.setFeedback(0.0f);
        disper.setDispersion(baseDisp);

        // sync their rate/depth initially
        syncSubModules(0.0f);
    }

    // ---------- Global ----------
    void setMix(float v)   { mix = std::clamp(v, 0.f, 1.f); }
    void setWidth(float v) { width = std::clamp(v, 0.f, 1.f); } // 0=mono, 1=wide

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ---------- Motion clock ----------
    void setRate(float r)   { baseRate = std::max(0.01f, r); }
    void setDepth(float d)  { baseDepth = std::clamp(d, 0.f, 1.f); }
    void setEnvAmount(float e) { envAmt = std::clamp(e, 0.f, 1.f); }
    void setEnvTime(float t)   { envCoef = std::exp(-1.0f / (std::max(0.001f, t) * (float)gam::sampleRate())); }

    // ---------- Sub-module mix amounts ----------
    void setMotionAmount(float v)     { amtMotion = std::clamp(v, 0.f, 1.f); }
    void setPhaseAmount(float v)      { amtPhase  = std::clamp(v, 0.f, 1.f); }
    void setDispersionAmount(float v) { amtDisp   = std::clamp(v, 0.f, 1.f); }

    // ---------- Spectral controls ----------
    void setBaseCutoff(float f)
    {
        baseCutoff = std::clamp(f, 40.f, 18000.f);
        motion.setBaseCutoff(baseCutoff);
    }

    void setResonance(float q)
    {
        baseRes = std::clamp(q, 0.1f, 1.8f);
        motion.setResonance(baseRes);
    }

    // Shared feedback (distributed to phase + dispersion)
    void setFeedback(float fb)
    {
        baseFB = std::clamp(fb, -0.95f, 0.95f);
    }

    // Dispersion amount passed to dispersive stage
    void setDispersion(float d)
    {
        baseDisp = std::clamp(d, 0.f, 1.f);
        disper.setDispersion(baseDisp);
    }

    // ---------------------------------------------------------
    // Mono process (runs mid only)
    // ---------------------------------------------------------
    float process(float x) override
    {
        float yL, yR;
        processStereo(x, x, yL, yR);
        return 0.5f * (yL + yR);
    }

    // ---------------------------------------------------------
    // Stereo process (recommended)
    // ---------------------------------------------------------
    void processStereo(float inL, float inR, float& outL, float& outR)
    {
        // ---- Input drive / envelope follower ----
        const float inputMid = 0.5f * (inL + inR);
        const float lvl = std::fabs(inputMid);
        env += (lvl - env) * (1.f - envCoef);

        // ---- Motion clock (LFO) ----
        // internal LFO phase in [0,1)
        phase += baseRate / (float)gam::sampleRate();
        if(phase >= 1.f) phase -= 1.f;

        // smooth, bounded LFO
        float lfo = std::sin(phase * 6.2831853f);
        lfoSmooth += (lfo - lfoSmooth) * 0.001f;
        lfo = lfoSmooth;

        // Combine LFO + env -> motion factor [-1..1]
        float motionCtl = lfo * baseDepth + env * envAmt;
        motionCtl = std::clamp(motionCtl, -1.f, 1.f);

        // Update submodules coherently from one motion controller
        syncSubModules(motionCtl);

        // ---- Mid/Side conversion ----
        float mid = inputMid;
        float side = 0.5f * (inL - inR);

        // width: 0 mono, 1 original, >1 not allowed here (but you can extend)
        side *= width;

        // ---- Process MID path ----
        float m = mid * im.process();

        // Motion filter adds dynamic “sweepable EQ” feel
        float m_motion = motion.process(m);

        // Phase shifter: swirl notches
        float m_phase = phaser.process(m_motion);

        // Dispersive: group-delay smear
        float m_disp  = disper.process(m_phase);

        // Blend stage contributions (in-series core, then parallel macro blend)
        // We keep the serial character, then allow bypass-weight via amounts.
        float m_out = m;
        m_out = lerp(m_out, m_motion, amtMotion);
        m_out = lerp(m_out, m_phase,  amtPhase);
        m_out = lerp(m_out, m_disp,   amtDisp);

        // ---- Process SIDE path (slightly different motion for stereo life) ----
        // Phase offset adds stereo motion without delays.
        float phaseR = phase + 0.25f; // +90°
        if(phaseR >= 1.f) phaseR -= 1.f;
        float lfoR = std::sin(phaseR * 6.2831853f);
        lfoRSmooth += (lfoR - lfoRSmooth) * 0.001f;
        lfoR = lfoRSmooth;

        float motionCtlR = lfoR * baseDepth + env * envAmt;
        motionCtlR = std::clamp(motionCtlR, -1.f, 1.f);

        // we re-sync for side with slightly different controller
        syncSubModulesSide(motionCtlR);

        float s = side * im.process();
        float s_motion = motion.process(s);
        float s_phase  = phaser.process(s_motion);
        float s_disp   = disper.process(s_phase);

        float s_out = s;
        s_out = lerp(s_out, s_motion, amtMotion);
        s_out = lerp(s_out, s_phase,  amtPhase);
        s_out = lerp(s_out, s_disp,   amtDisp);

        // ---- Feedback distribution (soft-sat for stability) ----
        // A tiny global feedback can make motion “bind together”.
        // We feed back mid only to avoid stereo weirdness.
        fbState = std::tanh(fbState) * baseFB + 0.1f * (m_out - fbState);
        m_out += 0.25f * fbState;

        // ---- Back to L/R ----
        float wetL = m_out + s_out;
        float wetR = m_out - s_out;

        // equal-power global mix
        float dryL = inL * std::cos(mix * 1.5707963f);
        float dryR = inR * std::cos(mix * 1.5707963f);
        float wetGain = std::sin(mix * 1.5707963f);

        outL = (dryL + wetL * wetGain) * am.process();
        outR = (dryR + wetR * wetGain) * am.process();
    }

private:
    // Sync submodules from one controller value in [-1,1]
    void syncSubModules(float motionCtl)
    {
        // MotionFilter controls
        motion.setRate(baseRate);
        motion.setDepth(std::fabs(motionCtl));      // intensity
        motion.setEnvAmount(envAmt);                // keep consistent

        // PhaseShifter controls
        phaser.setRate(baseRate);
        phaser.setDepth(baseDepth);
        phaser.setFeedback(baseFB * 0.6f);          // share feedback

        // Dispersive controls
        disper.setRate(baseRate * (0.8f + 0.6f * baseDepth));
        disper.setDepth(baseDepth);
        disper.setFeedback(baseFB);                 // smear likes more fb
        disper.setEnvAmount(envAmt);
        disper.setDispersion(baseDisp);
    }

    void syncSubModulesSide(float motionCtl)
    {
        // For stereo: same base but slight variations via motionCtl
        const float d = std::clamp(baseDepth * (0.85f + 0.15f * std::fabs(motionCtl)), 0.f, 1.f);

        motion.setRate(baseRate);
        motion.setDepth(d);
        motion.setEnvAmount(envAmt);

        phaser.setRate(baseRate);
        phaser.setDepth(d);
        phaser.setFeedback(baseFB * 0.5f);

        disper.setRate(baseRate * (0.85f + 0.4f * d));
        disper.setDepth(d);
        disper.setFeedback(baseFB);
        disper.setEnvAmount(envAmt);
        disper.setDispersion(baseDisp);
    }

    static inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }

private:
    // Submodules
    ModMotionFilter   motion;
    PhaseShifter      phaser;
    DispersivePhaser  disper;

    // Global params
    float mix   = 0.85f;
    float width = 0.75f;

    float baseRate  = 0.22f;
    float baseDepth = 0.80f;

    float envAmt  = 0.35f;
    float env     = 0.0f;
    float envCoef = 0.001f;

    float baseCutoff = 900.f;
    float baseRes    = 0.8f;

    float baseFB   = 0.35f;
    float baseDisp = 0.55f;

    float amtMotion = 0.55f;
    float amtPhase  = 0.55f;
    float amtDisp   = 0.45f;

    // Modulators
    Modulator im, am;

    // Internal motion state
    float phase     = 0.f;
    float lfoSmooth = 0.f;
    float lfoRSmooth = 0.f;

    // Global feedback state (mid)
    float fbState = 0.f;
};
