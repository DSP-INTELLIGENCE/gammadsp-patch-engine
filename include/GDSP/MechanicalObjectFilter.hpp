#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "GDSP_NonlinearModalResonator.hpp"
#include "GDSP_DispersionNetwork.hpp"
#include "GDSP_PhaseDiffuser.hpp"

// Small helper: soft saturation with drive
static inline float softSat(float x, float drive)
{
    return std::tanh(x * std::max(0.0f, drive));
}

// Simple 1-pole LP for smoothing control/energy
static inline float onePole(float x, float& z, float a)
{
    z += a * (x - z);
    return z;
}

class MechanicalObjectFilter : public Function
{
public:
    enum class ObjectType {
        METAL_BOX,
        SPRINGY_FRAME,
        GLASSY_THING,
        WOODEN_HOUSING,
        MACHINE_CHASSIS
    };

    MechanicalObjectFilter()
    : mix(1.0f)
    {
        setIM(1.0f);
        setAM(1.0f);

        // Defaults: "machine chassis"
        body.resize(14);
        body.setMix(1.0f);
        body.setDrive(1.6f);
        body.setCoupling(0.35f);

        dispersion.setMix(1.0f);
        dispersion.setDispersion(0.6f);
        dispersion.setFeedback(0.35f);
        dispersion.setDepth(0.25f);

        diffuser = PhaseDiffuser(10);
        diffuser.setMix(1.0f);
        diffuser.setDepth(0.7f);

        setObjectType(ObjectType::MACHINE_CHASSIS);

        setFB(0.0f);
        setDrive(1.6f);
        setCoupling(0.35f);
        setDispersion(0.6f);
        setDiffusion(0.7f);

        setFriction(0.35f);     // stick-slip amount
        setBacklash(0.20f);     // deadzone / slack
        setRattle(0.30f);       // micro-impacts amount
        setRattleRate(0.08f);   // probability base per sample (scaled)
        setTone(0.25f);         // brightness trim
        setMix(1.0f);
    }

    // ---------------- Macros ----------------
    void setObjectType(ObjectType t)
    {
        type = t;
        switch (t)
        {
            case ObjectType::METAL_BOX:
                configureModes(80.f, 1.55f, 0.10f);
                setDrive(2.0f);
                setCoupling(0.45f);
                setDispersion(0.85f);
                setDiffusion(0.55f);
                setFriction(0.20f);
                setBacklash(0.10f);
                setRattle(0.25f);
                setTone(0.10f);
                break;

            case ObjectType::SPRINGY_FRAME:
                configureModes(55.f, 1.25f, 0.18f);
                setDrive(1.4f);
                setCoupling(0.40f);
                setDispersion(0.55f);
                setDiffusion(0.80f);
                setFriction(0.50f);
                setBacklash(0.35f);
                setRattle(0.40f);
                setTone(0.35f);
                break;

            case ObjectType::GLASSY_THING:
                configureModes(120.f, 1.75f, 0.06f);
                setDrive(1.8f);
                setCoupling(0.30f);
                setDispersion(0.90f);
                setDiffusion(0.35f);
                setFriction(0.15f);
                setBacklash(0.08f);
                setRattle(0.20f);
                setTone(0.05f);
                break;

            case ObjectType::WOODEN_HOUSING:
                configureModes(70.f, 1.15f, 0.22f);
                setDrive(1.2f);
                setCoupling(0.25f);
                setDispersion(0.25f);
                setDiffusion(0.85f);
                setFriction(0.45f);
                setBacklash(0.25f);
                setRattle(0.30f);
                setTone(0.55f);
                break;

            case ObjectType::MACHINE_CHASSIS:
            default:
                configureModes(90.f, 1.45f, 0.14f);
                setDrive(1.6f);
                setCoupling(0.35f);
                setDispersion(0.60f);
                setDiffusion(0.70f);
                setFriction(0.35f);
                setBacklash(0.20f);
                setRattle(0.30f);
                setTone(0.25f);
                break;
        }
    }

    // ---------------- Primary controls ----------------
    void setMix(float m)      { mix = std::clamp(m, 0.0f, 1.0f); }
    void setFB(float f)       { fb = std::clamp(f, -0.99f, 0.99f); }

    void setDrive(float d)    { drive = std::max(0.0f, d); body.setDrive(drive); }
    void setCoupling(float c) { coupling = std::clamp(c, 0.0f, 1.0f); body.setCoupling(coupling); }

    void setDispersion(float d) { disp = std::clamp(d, 0.0f, 1.0f); dispersion.setDispersion(disp); }
    void setDiffusion(float d)  { diff = std::clamp(d, 0.0f, 1.0f); diffuser.setDepth(diff); }

    // friction: stick-slip strength (0..1)
    void setFriction(float f) { friction = std::clamp(f, 0.0f, 1.0f); }
    // backlash: deadzone slack (0..1)
    void setBacklash(float b) { backlash = std::clamp(b, 0.0f, 1.0f); }
    // rattle: micro-impact strength (0..1)
    void setRattle(float r)   { rattle = std::clamp(r, 0.0f, 1.0f); }
    // base probability scaler (0..1-ish); internally scaled by energy & SR
    void setRattleRate(float p) { rattleRate = std::clamp(p, 0.0f, 1.0f); }

    // tone: simple HF trimming (0 bright .. 1 dark)
    void setTone(float t) { tone = std::clamp(t, 0.0f, 1.0f); }

    // ---------------- Gain stages ----------------
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // ---------------- Deep modulation (optional) ----------------
    void setFBM(float v)   { fbm.set(v); }
    void setDRM(float v)   { drm.set(v); }     // drive mod
    void setDPM(float v)   { dpm.set(v); }     // dispersion mod
    void setDFM(float v)   { dfm.set(v); }     // diffusion mod
    void setFRM(float v)   { frm.set(v); }     // friction mod
    void setRATM(float v)  { ratm.set(v); }    // rattle amount mod
    void setMIXM(float v)  { mixm.set(v); }

    float process(float x) override
    {
        // ---- apply modulated macro controls safely ----
        float _fbm  = fbm.process();
        float _drm  = drm.process();
        float _dpm  = dpm.process();
        float _dfm  = dfm.process();
        float _frm  = frm.process();
        float _ratm = ratm.process();
        float _mxm  = mixm.process();

        float mFB   = std::clamp(fb + fb * _fbm, -0.99f, 0.99f);
        float mDrv  = std::max(0.0f, drive + drive * _drm);
        float mDisp = std::clamp(disp + disp * _dpm, 0.0f, 1.0f);
        float mDiff = std::clamp(diff + diff * _dfm, 0.0f, 1.0f);
        float mFric = std::clamp(friction + friction * _frm, 0.0f, 1.0f);
        float mRat  = std::clamp(rattle + rattle * _ratm, 0.0f, 1.0f);
        float mMix  = std::clamp(mix + mix * _mxm, 0.0f, 1.0f);

        body.setDrive(mDrv);
        dispersion.setDispersion(mDisp);
        diffuser.setDepth(mDiff);

        // ---- input + feedback ----
        float in = x * im.process() + lastOut * mFB;

        // ---- energy estimate (for rattle trigger & friction behavior) ----
        float e = std::fabs(in);
        // smooth energy (fast-ish)
        float sr = (float)gam::sampleRate();
        float aE = 1.0f - std::exp(-1.0f / (0.01f * sr)); // ~10ms
        onePole(e, energyZ, aE);

        // ---- backlash (deadzone) to emulate slack in mechanics ----
        float dz = backlash * 0.5f; // deadzone width
        float inDZ = (std::fabs(in) < dz) ? 0.0f : (in - std::copysign(dz, in));

        // ---- stick-slip friction (nonlinear damping + slip injection) ----
        // Idea: when velocity (signal) is small, "stick" (more damping),
        // when large, "slip" (adds roughness). This is a stylized model.
        float vel = inDZ - prevIn;
        prevIn = inDZ;

        float slip = softSat(vel, 2.0f + 8.0f * mFric);              // roughness
        float stick = 1.0f - (mFric * (1.0f - std::min(1.0f, std::fabs(vel) * 20.0f)));
        float mech = (inDZ * stick) + (slip * (0.15f * mFric));

        // ---- rattle micro-impacts (energy-triggered impulses) ----
        // Probability grows with energy; impulses excite body strongly.
        float p = (rattleRate * 0.0025f) * (1.0f + 8.0f * energyZ); // per-sample
        p = std::clamp(p, 0.0f, 0.25f); // safety

        if (rand01() < p)
        {
            float imp = (randSigned() * (0.3f + 0.7f * energyZ)) * mRat;
            // make it clicky but bounded
            mech += softSat(imp, 3.0f + 6.0f * mRat);
        }

        // ---- body resonance + dispersion + diffusion ----
        float y = body.process(mech);
        y = dispersion.process(y);
        y = diffuser.process(y);

        // ---- simple tone trim (one-pole LP on output) ----
        if (tone > 0.0f)
        {
            float aT = 1.0f - std::exp(-1.0f / ((0.002f + 0.05f * tone) * sr)); // 2ms..52ms
            onePole(y, toneZ, aT);
            y = toneZ;
        }

        // ---- output mix + gain ----
        float out = x * (1.0f - mMix) + y * mMix;
        out *= am.process();

        lastOut = out;
        return out;
    }

private:
    // Create a plausible inharmonic modal set for “mechanical objects”
    // baseF: base mode spacing (Hz-ish)
    // inharm: inharmonicity factor (1=more harmonic; >1 more metallic)
    // damp: 0..1 (higher = shorter ring)
    void configureModes(float baseF, float inharm, float damp)
    {
        // map damping to decay coefficient for your modal model
        float decayBase = 0.99985f - 0.02f * std::clamp(damp, 0.0f, 1.0f);
        decayBase = std::clamp(decayBase, 0.90f, 0.99999f);

        const int M = 14;
        body.resize(M);

        for (int i = 0; i < M; ++i)
        {
            float n = float(i + 1);

            // inharmonic stretched series (good for metal/glass)
            float f = baseF * std::pow(n, 1.25f) * std::pow(inharm, 0.25f * n / float(M));

            // faster decay for higher modes
            float d = std::clamp(decayBase - 0.00025f * n, 0.85f, 0.99999f);

            // gain roll-off
            float g = 1.0f / std::pow(n, 0.65f);

            body.setMode(i, f, d, g);
        }
    }

    // RNG helpers (fast, deterministic)
    float rand01()
    {
        rng = 1664525u * rng + 1013904223u;
        return (rng >> 8) * (1.0f / 16777216.0f); // [0,1)
    }
    float randSigned() { return rand01() * 2.0f - 1.0f; }

private:
    ObjectType type = ObjectType::MACHINE_CHASSIS;

    // Blocks
    NonlinearModalResonator body;
    DispersionNetwork dispersion;
    PhaseDiffuser diffuser;

    // Macros
    float mix = 1.0f;
    float fb  = 0.0f;

    float drive    = 1.6f;
    float coupling = 0.35f;

    float disp = 0.6f;
    float diff = 0.7f;

    float friction   = 0.35f;
    float backlash   = 0.20f;
    float rattle     = 0.30f;
    float rattleRate = 0.08f;

    float tone = 0.25f;

    // State
    float lastOut = 0.0f;
    float prevIn  = 0.0f;
    float energyZ = 0.0f;
    float toneZ   = 0.0f;

    // Gain stages
    Modulator im, am;

    // Modulation depths
    Modulator fbm, drm, dpm, dfm, frm, ratm, mixm;

    // RNG
    uint32_t rng = 0x12345678u;
};
