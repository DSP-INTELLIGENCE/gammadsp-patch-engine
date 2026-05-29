#pragma once
#include "TPT1Pole.hpp"
#include "TriodeStage2.hpp"

struct AnalogPowerAmpBlock
{
    // =========================================================
    // Core stages
    // =========================================================

    TriodeStage triodeP;   // push
    TriodeStage triodeN;   // pull

    // =========================================================
    // Support filters (LINEAR)
    // =========================================================

    TPT1Pole sagLP;        // power supply sag detector
    TPT1Pole dcBlock;      // output DC blocker
    TPT1Pole nfbHP;        // presence network (high-pass)

    // =========================================================
    // Parameters
    // =========================================================

    float drive      = 1.0f;   // power amp drive
    float sagAmount  = 0.5f;   // supply sag depth
    float xfmrDrive  = 1.0f;   // transformer saturation

    float master     = 1.0f;   // MASTER VOLUME (linear, BEFORE PA)
    float feedback   = 0.15f;  // negative feedback amount
    float presence   = 0.35f;  // 0..1 (HF feedback reduction)

    // =========================================================
    // Modulators
    // =========================================================

    Modulator driveMod;
    Modulator sagMod;
    Modulator xfmrMod;
    Modulator masterMod;

    // =========================================================
    // Internal state
    // =========================================================

    float sagState = 1.0f;
    float lastOut  = 0.0f;

    // =========================================================
    // Lifecycle
    // =========================================================

    void init(float fs)
    {
        triodeP.init(fs);
        triodeN.init(fs);

        // Push–pull mismatch (intentional)
        triodeP.drive = 1.0f;
        triodeN.drive = 0.97f;

        sagLP.setLPCut(5.0f, fs);      // ~5 Hz supply response
        sagLP.reset(0.0f);

        dcBlock.setHPCut(10.0f, fs);   // transformer HP
        dcBlock.reset(0.0f);

        nfbHP.setHPCut(1200.0f, fs);   // presence corner
        nfbHP.reset(0.0f);

        reset();
    }

    void reset()
    {
        triodeP.reset();
        triodeN.reset();
        sagLP.reset(0.0f);
        dcBlock.reset(0.0f);
        nfbHP.reset(0.0f);

        sagState = 1.0f;
        lastOut  = 0.0f;
    }

    // =========================================================
    // Processing
    // =========================================================

    inline float process(float x)
    {
        // -----------------------------------------------------
        // MASTER VOLUME (APPLIED BEFORE POWER AMP)
        // -----------------------------------------------------
        float m = master * (1.0f + masterMod.process());
        x *= std::max(0.0f, m);

        // -----------------------------------------------------
        // NEGATIVE FEEDBACK + PRESENCE
        // -----------------------------------------------------
        float fb = lastOut;

        float pres = std::clamp(presence, 0.f, 1.f);
        float fbHF = nfbHP.processHP(fb);

        // Presence = less HF feedback
        float fbSignal = (1.0f - pres) * fb + pres * fbHF;

        x -= std::max(0.f, feedback) * fbSignal;

        // -----------------------------------------------------
        // PHASE SPLITTER (imperfect)
        // -----------------------------------------------------
        float xp =  x;
        float xn = -x * 0.97f;

        // -----------------------------------------------------
        // DRIVE
        // -----------------------------------------------------
        float d = drive * (1.0f + driveMod.process());
        xp *= d;
        xn *= d;

        // -----------------------------------------------------
        // PUSH–PULL TRIODES
        // -----------------------------------------------------
        float yp = triodeP.process(xp);
        float yn = triodeN.process(xn);
        float y  = yp - yn;

        // -----------------------------------------------------
        // POWER SUPPLY SAG (CURRENT-BASED)
        // -----------------------------------------------------
        float env = std::fabs(yp) + std::fabs(yn);
        float sagEnv = sagLP.processLP(env);

        float sagDepth = std::max(0.f, sagAmount * (1.f + sagMod.process()));
        float sagTarget = 1.f / (1.f + sagDepth * sagEnv);

        sagState += 0.0015f * (sagTarget - sagState);
        y *= sagState;

        // -----------------------------------------------------
        // OUTPUT TRANSFORMER SATURATION (ASYMMETRIC)
        // -----------------------------------------------------
        float xd = std::max(0.1f, xfmrDrive * (1.f + xfmrMod.process()));
        float t  = y * xd;

        y = (t >= 0.f) ? std::tanh(t)
                       : 0.85f * std::tanh(t);

        // -----------------------------------------------------
        // DC BLOCK (LINEAR)
        // -----------------------------------------------------
        y = dcBlock.processHP(y);

        lastOut = y;
        return y;
    }
};
