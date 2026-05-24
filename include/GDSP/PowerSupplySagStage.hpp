#pragma once
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------------
// PowerSupplySagStage (drop-in like TriodeTube)
// - Virtual PSU sag + recovery (global constraint / "glue")
// - Program-dependent headroom reduction + bias shift
// - Feedback-safe (smooth tanh / soft-limit), no hard corners
// - No allocations, minimal state
//
// Musical controls (0..1 unless noted):
//   Sag        : how easily supply collapses (depth)
//   Cap        : recovery time / "cap size" (bigger = slower recovery)
//   Load       : how strongly audio draws power (sensitivity)
//   BiasCouple : how much sag shifts symmetry (even harmonics / operating point)
//   Mix        : dry/wet
//   BaseHeadroom (>=0.05): nominal headroom scaling applied to saturation
//
// Modulation:
// - im/am are audio modulators (same convention as your FrequencyShifter)
// - sagMod/capMod/loadMod/biasMod/mixMod are scalar modulators (no-arg process)
//
// Typical placement:
//   TriodeTube -> MagneticTransformerStage -> PowerSupplySagStage -> ...
// Or inside feedback paths to add "amp-like" breathing and stabilization.
// -----------------------------------------------------------------------------
class PowerSupplySagStage : public Function
{
public:
    PowerSupplySagStage()
    {
        reset();
        setSampleRate((float)gam::sampleRate());

        setSag(0.55f);
        setCap(0.55f);
        setLoad(0.55f);
        setBiasCouple(0.35f);
        setMix(1.0f);

        setBaseHeadroom(1.0f);
        setHFSoftening(0.25f);
        setDCReject(0.0f);

        // PSU corner defaults (envelope characteristics)
        setAttackMs(15.0f);
        setReleaseMs(450.0f);
    }

    void setSampleRate(float sr)
    {
        mSR = std::max(8000.0f, sr);
        updateEnvCoeffs();
        updateCapCoeffs();
        updateHF();
        updateCtlSmoothing();
    }

    void reset()
    {
        mEnv = 0.0f;
        mVpsu = 1.0f;   // normalized supply voltage
        mBiasShift = 0.0f;

        mLP = 0.0f;
        mDCZ = 0.0f;

        mSagLP = mSag;
        mCapLP = mCap;
        mLoadLP = mLoad;
        mBiasCoupleLP = mBiasCouple;
        mMixLP = mMix;

        mCapCached = -1.0f;
        mLastOut = 0.0f;
    }

    // ----- Controls -----
    void setSag(float v)        { mSag = std::clamp(v, 0.0f, 1.0f); }          // depth
    void setCap(float v)        { mCap = std::clamp(v, 0.0f, 1.0f); updateCapCoeffs(); } // recovery
    void setLoad(float v)       { mLoad = std::clamp(v, 0.0f, 1.0f); }         // sensitivity
    void setBiasCouple(float v) { mBiasCouple = std::clamp(v, 0.0f, 1.0f); }   // asymmetry from sag
    void setMix(float v)        { mMix = std::clamp(v, 0.0f, 1.0f); }          // dry/wet

    void setBaseHeadroom(float v) { mBaseHead = std::max(0.05f, v); }          // >=0.05
    void setHFSoftening(float v)  { mHFSoft = std::clamp(v, 0.0f, 1.0f); updateHF(); }

    // Optional internal DC reject (you can also use BlockDC externally)
    void setDCReject(float v01) { mDCReject = std::clamp(v01, 0.0f, 1.0f); }

    // Envelope characteristics (ms)
    void setAttackMs(float ms)  { mAtkMs = std::clamp(ms, 0.1f, 200.0f); updateEnvCoeffs(); }
    void setReleaseMs(float ms) { mRelMs = std::clamp(ms, 1.0f, 5000.0f); updateEnvCoeffs(); }

    // ----- Scalar modulation depths (typically [-1,1]) -----
    void setSagMod(float v)        { sagMod.set(v); }
    void setCapMod(float v)        { capMod.set(v); }
    void setLoadMod(float v)       { loadMod.set(v); }
    void setBiasCoupleMod(float v) { biasMod.set(v); }
    void setMixMod(float v)        { mixMod.set(v); }

    // ----- Audio modulators -----
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Debug/meters
    float envState()  const { return mEnv; }
    float vpsuState() const { return mVpsu; }
    float biasState() const { return mBiasShift; }

    float process(float x) override
    {
        const float dry = x;
        x = im.process(x);

        // --- Modulated parameters ---
        float sag  = mSag        + sagMod.process();
        float cap  = mCap        + capMod.process();
        float load = mLoad       + loadMod.process();
        float bc   = mBiasCouple + biasMod.process();
        float mix  = mMix        + mixMod.process();

        sag  = std::clamp(sag,  0.0f, 1.0f);
        cap  = std::clamp(cap,  0.0f, 1.0f);
        load = std::clamp(load, 0.0f, 1.0f);
        bc   = std::clamp(bc,   0.0f, 1.0f);
        mix  = std::clamp(mix,  0.0f, 1.0f);

        // --- Smooth controls (automation-safe) ---
        mSagLP        += mCtlAlpha * (sag  - mSagLP);
        mCapLP        += mCtlAlpha * (cap  - mCapLP);
        mLoadLP       += mCtlAlpha * (load - mLoadLP);
        mBiasCoupleLP += mCtlAlpha * (bc   - mBiasCoupleLP);
        mMixLP        += mCtlAlpha * (mix  - mMixLP);

        // Cap changes time constants; update lazily
        if (std::fabs(mCapLP - mCapCached) > 1e-4f)
        {
            mCapCached = mCapLP;
            updateCapCoeffsFromCap(mCapLP);
        }

        // --- Envelope follower (power draw proxy) ---
        // Use |x| with attack/release (fast attack, slow release)
        const float ax = std::fabs(x);
        const float eAlpha = (ax > mEnv) ? mEnvAtkA : mEnvRelA;
        mEnv += eAlpha * (ax - mEnv);

        // --- Sag mapping ---
        // Make sag nonlinear: small env -> gentle, big env -> much stronger
        // "load" increases sensitivity; "sag" sets depth.
        const float sens = 0.6f + 3.0f * mLoadLP;           // ~0.6..3.6
        const float draw = 1.0f - std::exp(-sens * mEnv);   // 0..~1

        // Target PSU voltage (bounded)
        // More sag -> lower Vpsu under load.
        float vTarget = 1.0f - (0.85f * mSagLP) * draw;

        // Prevent collapsing to zero (real supplies bottom out but not instantly)
        vTarget = std::clamp(vTarget, 0.25f, 1.05f);

        // --- Recovery dynamics (capacitor behavior) ---
        // Deep sag recovers slower than shallow sag (program-dependent).
        const float depth = 1.0f - mVpsu;
        const float slowFactor = 1.0f + 2.5f * depth;     // more sag => slower recovery

        // One-pole toward target; different rates depending on direction:
        // - sag down is faster
        // - recovery up is slower (cap)
        const float aDown = mSagDownA;
        const float aUp   = std::clamp(mSagUpA / slowFactor, 0.000001f, 0.2f);

        const float aV = (vTarget < mVpsu) ? aDown : aUp;
        mVpsu += aV * (vTarget - mVpsu);

        // --- Bias shift from sag (even harmonics / operating point movement) ---
        // When supply drops, operating point tends to skew and "thicken".
        // Keep it small and smooth.
        const float sagAmt = (1.0f - mVpsu); // 0..~0.75
        const float biasT  = (mBiasCoupleLP * 0.6f) * sagAmt;
        mBiasShift += mBiasA * (biasT - mBiasShift);

        // --- Apply PSU constraint to signal ---
        // 1) Dynamic headroom reduction (starvation)
        const float head = std::max(0.05f, mBaseHead * (0.65f + 0.70f * mVpsu));
        float y = x;

        // 2) Bias coupling adds subtle asymmetry (even harmonics)
        y += mBiasShift * y * y;

        // 3) Starved saturation: less headroom when Vpsu low
        y = softSat(y, head);

        // 4) HF softening under heavy draw (subtle)
        if (mHFSoft > 0.0f)
        {
            // Simple 1-pole LP on output to emulate supply-induced HF softness.
            // Cutoff shifts down with sag and HFSoft amount.
            // Update g cheaply per sample using a linear approximation around a fixed corner.
            const float sagHF = std::clamp(sagAmt * (0.25f + 0.75f * mHFSoft), 0.0f, 0.8f);

            // Effective LP coefficient increases as sag increases (more smoothing).
            const float g = std::clamp(mHFG + sagHF * 0.08f, 0.0001f, 0.25f);

            mLP += g * (y - mLP);
            // Mix in only a portion so HF doesn't vanish
            y = y * (1.0f - 0.35f * mHFSoft) + mLP * (0.35f * mHFSoft);
        }

        // --- Optional DC reject ---
        if (mDCReject > 0.0f)
        {
            const float r = 0.995f + (1.0f - mDCReject) * 0.0045f; // ~0.995..0.9995
            const float out = y - mDCZ + r * mDCZ;
            mDCZ = y;
            y = out;
        }

        // Dry/Wet blend
        y = dry + (y - dry) * mMixLP;

        // Output audio modulation
        return am.process(y);
    }

    // Public modulators (match your style)
    Modulator sagMod, capMod, loadMod, biasMod, mixMod;
    Modulator im, am;

private:
    static inline float softSat(float x, float head)
    {
        // Smooth saturation with headroom scaling
        const float g = 1.0f / std::max(1e-6f, head);
        return head * std::tanh(x * g);
    }

    void updateCtlSmoothing()
    {
        // ~5ms parameter smoothing
        const float tau = 0.005f;
        mCtlAlpha = 1.0f - std::exp(-1.0f / (tau * mSR));
    }

    void updateEnvCoeffs()
    {
        // Attack/release for envelope follower
        // alpha = 1 - exp(-1/(tau*sr)), tau in seconds
        const float atk = std::max(0.0001f, mAtkMs * 0.001f);
        const float rel = std::max(0.001f,  mRelMs * 0.001f);

        mEnvAtkA = 1.0f - std::exp(-1.0f / (atk * mSR));
        mEnvRelA = 1.0f - std::exp(-1.0f / (rel * mSR));
    }

    void updateCapCoeffs()
    {
        updateCapCoeffsFromCap(mCap);
    }

    void updateCapCoeffsFromCap(float cap01)
    {
        // Cap controls sag recovery speed:
        //   cap=0 -> snappy recovery
        //   cap=1 -> slow, gluey recovery
        //
        // Sag-down (droop) is usually faster than recovery.
        // Down: ~2ms..30ms
        // Up  : ~30ms..2500ms
        const float downMs = 2.0f   + 28.0f   * cap01;
        const float upMs   = 30.0f  + 2470.0f * cap01;

        const float downTau = 0.001f * downMs;
        const float upTau   = 0.001f * upMs;

        mSagDownA = 1.0f - std::exp(-1.0f / (downTau * mSR));
        mSagUpA   = 1.0f - std::exp(-1.0f / (upTau   * mSR));

        // Bias shift follows sag fairly slowly (~80ms..700ms)
        const float biasMs = 80.0f + 620.0f * cap01;
        const float biasTau = 0.001f * biasMs;
        mBiasA = 1.0f - std::exp(-1.0f / (biasTau * mSR));

        updateCtlSmoothing();
    }

    void updateHF()
    {
        // Base HF smoothing coefficient for the optional output LP
        // Higher SR -> smaller coefficient for same subjective corner.
        // Choose a nominal corner ~12kHz.
        const float fc = 12000.0f;
        const float x = -2.0f * 3.14159265358979323846f * fc / mSR;
        mHFG = 1.0f - std::exp(x);
        mHFG = std::clamp(mHFG, 0.00005f, 0.15f);
    }

private:
    float mSR = 48000.0f;

    // Controls
    float mSag        = 0.55f;
    float mCap        = 0.55f;
    float mLoad       = 0.55f;
    float mBiasCouple = 0.35f;
    float mMix        = 1.0f;

    float mBaseHead = 1.0f;
    float mHFSoft   = 0.25f;
    float mDCReject = 0.0f;

    // Envelope timing
    float mAtkMs = 15.0f;
    float mRelMs = 450.0f;

    // Smoothed controls
    float mSagLP = 0.55f;
    float mCapLP = 0.55f;
    float mLoadLP = 0.55f;
    float mBiasCoupleLP = 0.35f;
    float mMixLP = 1.0f;

    float mCtlAlpha = 0.02f;

    // State
    float mEnv = 0.0f;
    float mVpsu = 1.0f;
    float mBiasShift = 0.0f;

    // Optional HF softening state
    float mLP = 0.0f;
    float mHFG = 0.01f;

    // Optional DC reject state
    float mDCZ = 0.0f;

    // Cached
    float mCapCached = -1.0f;
    float mLastOut = 0.0f;

    // Envelope coefficients
    float mEnvAtkA = 0.05f;
    float mEnvRelA = 0.005f;

    // Sag dynamics coefficients
    float mSagDownA = 0.05f;
    float mSagUpA   = 0.001f;

    // Bias follow coefficient
    float mBiasA = 0.002f;
};
