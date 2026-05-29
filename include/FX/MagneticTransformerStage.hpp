#pragma once
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------------
// MagneticTransformerStage (drop-in like TriodeTube)
// - Stateful "iron" output transformer / power interaction stage
// - LF saturates first (flux / core model), HF stays clearer
// - Hysteresis + recovery ("iron") + dynamic headroom ("load")
// - Smooth, feedback-safe nonlinearity (tanh-based), no hard corners
//
// Controls (musical):
//   Drive : how hard you hit the transformer
//   Size  : core size / saturation threshold (bigger = more headroom)
//   Iron  : hysteresis + recovery time (more = thicker, slower memory)
//   Load  : dynamic headroom / compression feel (more = softer, heavier)
//   Mix   : dry/wet blend
//
// Modulation:
// - im/am are audio modulators (same convention as your FrequencyShifter)
// - driveMod/sizeMod/ironMod/loadMod/mixMod are scalar modulators (no-arg process)
//
// Notes:
// - Intended to sit after TriodeTube or inside feedback paths.
// - Flux/hysteresis update occurs as part of process() but remains smooth.
// -----------------------------------------------------------------------------
class MagneticTransformerStage : public Function
{
public:
    MagneticTransformerStage()
    {
        reset();
        setSampleRate((float)gam::sampleRate());

        setDrive(1.0f);
        setSize(0.6f);
        setIron(0.5f);
        setLoad(0.5f);
        setMix(1.0f);

        setLFCornerHz(180.0f);    // transformer "core" is mostly LF-driven
        setPhaseSmear(0.25f);     // subtle time smear
        setDCReject(0.0f);        // optional (you can also place BlockDC after)
    }

    void setSampleRate(float sr)
    {
        mSR = std::max(8000.0f, sr);
        updateSplit();
        updateMemoryCoeffs();
        updatePhase();
    }

    void reset()
    {
        mFlux = 0.0f;
        mHys  = 0.0f;
        mEnv  = 0.0f;

        mLP   = 0.0f;

        mAPz1 = 0.0f;
        mAPz2 = 0.0f;

        mDCZ  = 0.0f;

        // reset smoothed controls to current targets
        mDriveLP = mDrive;
        mSizeLP  = mSize;
        mIronLP  = mIron;
        mLoadLP  = mLoad;
        mMixLP   = mMix;
    }

    // ----- Controls (0..1 unless noted) -----
    void setDrive(float v)  { mDrive = std::max(0.0f, v); }              // >=0 (1 = nominal)
    void setSize(float v)   { mSize  = std::clamp(v, 0.0f, 1.0f); }      // bigger core
    void setIron(float v)   { mIron  = std::clamp(v, 0.0f, 1.0f); updateMemoryCoeffs(); }
    void setLoad(float v)   { mLoad  = std::clamp(v, 0.0f, 1.0f); }      // dynamic headroom/compression
    void setMix(float v)    { mMix   = std::clamp(v, 0.0f, 1.0f); }      // dry/wet

    // LF/HF split corner (Hz) - where the "core" starts dominating
    void setLFCornerHz(float hz)
    {
        mLFCorner = std::clamp(hz, 10.0f, 0.45f * mSR);
        updateSplit();
    }

    // Subtle phase smear (0..1). 0 = off.
    void setPhaseSmear(float v)
    {
        mPhase = std::clamp(v, 0.0f, 1.0f);
        updatePhase();
    }

    // Optional internal DC reject (0..1). 0 = off, 1 = stronger.
    void setDCReject(float v01)
    {
        mDCReject = std::clamp(v01, 0.0f, 1.0f);
    }

    // ----- Parameter modulators (scalar) -----
    void setDriveMod(float v) { driveMod.set(v); }  // typically [-1,1]
    void setSizeMod(float v)  { sizeMod.set(v);  }
    void setIronMod(float v)  { ironMod.set(v);  }
    void setLoadMod(float v)  { loadMod.set(v);  }
    void setMixMod(float v)   { mixMod.set(v);   }

    // ----- Audio modulators -----
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Debug/meters
    float fluxState() const { return mFlux; }
    float hysState()  const { return mHys;  }
    float envState()  const { return mEnv;  }

    float process(float x) override
    {
        // Audio-rate input modulation / gain staging
        const float dry = x;
        x = im.process(x);

        // --- Smooth controls (zipper-safe) ---
        // Modulators are assumed to be scalar signals (no-arg process) in [-1,1].
        float drv = mDrive * (1.0f + driveMod.process());
        float siz = mSize  + sizeMod.process();
        float irn = mIron  + ironMod.process();
        float lod = mLoad  + loadMod.process();
        float mix = mMix   + mixMod.process();

        drv = std::max(0.0f, drv);
        siz = std::clamp(siz, 0.0f, 1.0f);
        irn = std::clamp(irn, 0.0f, 1.0f);
        lod = std::clamp(lod, 0.0f, 1.0f);
        mix = std::clamp(mix, 0.0f, 1.0f);

        // One-pole smoothing (fast enough for automation, slow enough to kill zipper)
        // If you want “analog motion” you can make rise/fall different later.
        mDriveLP += mCtlAlpha * (drv - mDriveLP);
        mSizeLP  += mCtlAlpha * (siz - mSizeLP);
        mIronLP  += mCtlAlpha * (irn - mIronLP);
        mLoadLP  += mCtlAlpha * (lod - mLoadLP);
        mMixLP   += mCtlAlpha * (mix - mMixLP);

        // If Iron is moving, update memory coefficients lazily
        if (std::fabs(mIronLP - mIronCached) > 1e-4f)
        {
            mIronCached = mIronLP;
            updateMemoryCoeffsFromIron(mIronLP);
        }

        // --- Drive into transformer ---
        float in = x * mDriveLP;

        // Envelope (drives dynamic headroom and copper loss)
        const float a = std::fabs(in);
        mEnv += (a - mEnv) * mEnvAlpha;

        // --- LF/HF split (simple 1-pole LP) ---
        // LP: y += g*(x - y)
        mLP += mSplitG * (in - mLP);
        float lf = mLP;
        float hf = in - lf;

        // --- Core / flux model (LF-dominant) ---
        // Core "size" maps to headroom. Bigger core -> more headroom.
        // Load reduces headroom dynamically with energy (program-dependent compression).
        const float baseHead = 0.35f + 1.65f * mSizeLP;              // ~0.35..2.0
        const float loadComp = 1.0f / (1.0f + (0.8f * mLoadLP) * mEnv);
        float head = std::max(0.05f, baseHead * loadComp);

        // Flux integrates LF energy with leak (recovery). More iron -> slower recovery & more memory.
        // Use a bounded integrator to avoid runaway DC drift.
        const float fluxIn = lf * mFluxDrive;
        mFlux = mFlux * mFluxLeak + fluxIn * (1.0f - mFluxLeak);
        mFlux = softLimit(mFlux, 3.0f); // keep it bounded

        // Hysteresis follows flux with additional lag, adding "magnetic memory"
        mHys = mHys * mHysLeak + mFlux * (1.0f - mHysLeak);

        // Hysteresis injection into the core drive (adds asymmetry & thickness)
        const float hysAmt = 0.05f + 0.65f * mIronLP;               // ~0.05..0.70
        float coreDrive = lf + hysAmt * mHys;

        // Core saturation (smooth, feedback-safe)
        float lfSat = softSat(coreDrive, head);

        // --- HF behavior (copper loss / HF softening under drive) ---
        // Under higher energy and load, HF loses a bit (transformers tend to soften top on hit).
        const float hfLoss = (0.02f + 0.20f * mLoadLP) * (0.25f + 0.75f * mEnv);
        hf *= (1.0f - std::clamp(hfLoss, 0.0f, 0.35f));

        // Recombine
        float y = lfSat + hf;

        // --- Subtle phase smear (2 cascaded allpass) ---
        if (mPhase > 0.0f)
        {
            y = allpass(y, mAP_a1, mAPz1);
            y = allpass(y, mAP_a2, mAPz2);
        }

        // --- Optional DC reject (very gentle HP) ---
        if (mDCReject > 0.0f)
        {
            // leaky highpass: y = x - z + r*z
            const float r = 0.995f + (1.0f - mDCReject) * 0.0045f; // ~0.995..0.9995
            const float out = y - mDCZ + r * mDCZ;
            mDCZ = y;
            y = out;
        }

        // Dry/Wet
        y = dry + (y - dry) * mMixLP;

        // Output modulation
        return am.process(y);
    }

    // Public modulators (match your style)
    Modulator driveMod, sizeMod, ironMod, loadMod, mixMod;
    Modulator im, am;

private:
    static inline float softSat(float x, float head)
    {
        // tanh saturation with adjustable headroom
        const float g = 1.0f / std::max(1e-6f, head);
        return head * std::tanh(x * g);
    }

    static inline float softLimit(float x, float lim)
    {
        // Smooth limiter using tanh (C∞), keeps state bounded
        const float g = 1.0f / std::max(1e-6f, lim);
        return lim * std::tanh(x * g);
    }

    static inline float allpass(float x, float a, float& z)
    {
        // 1st-order allpass: y = -a*x + z; z = x + a*y
        const float y = -a * x + z;
        z = x + a * y;
        return y;
    }

    void updateSplit()
    {
        // 1-pole LP coefficient
        // g = 1 - exp(-2*pi*fc/fs)
        const float fc = std::max(10.0f, mLFCorner);
        const float x = -2.0f * 3.14159265358979323846f * fc / mSR;
        mSplitG = 1.0f - std::exp(x);
        mSplitG = std::clamp(mSplitG, 0.00001f, 0.99999f);
    }

    void updatePhase()
    {
        // Map phase smear amount into gentle allpass coefficients.
        // Small a => small phase shift. Keep stable: |a|<1.
        const float amt = mPhase;
        mAP_a1 = 0.05f + 0.35f * amt;       // ~0.05..0.40
        mAP_a2 = 0.03f + 0.22f * amt;       // ~0.03..0.25
        mAP_a1 = std::clamp(mAP_a1, 0.0f, 0.95f);
        mAP_a2 = std::clamp(mAP_a2, 0.0f, 0.95f);
    }

    void updateMemoryCoeffs()
    {
        updateMemoryCoeffsFromIron(mIron);
    }

    void updateMemoryCoeffsFromIron(float iron01)
    {
        // Iron controls recovery times:
        // flux:  ~20ms .. 900ms
        // hys :  ~120ms .. 3000ms
        const float fluxMs = 20.0f  + 880.0f  * iron01;
        const float hysMs  = 120.0f + 2880.0f * iron01;

        mFluxLeak = std::exp(-1.0f / (0.001f * fluxMs * mSR));
        mHysLeak  = std::exp(-1.0f / (0.001f * hysMs  * mSR));

        // How strongly LF drives flux. More iron -> slightly more “magnetization”.
        mFluxDrive = 0.65f + 0.55f * iron01; // ~0.65..1.20

        // Control smoothing alpha (fixed automation safety; can be made program-dependent)
        // ~5ms smoothing
        const float tau = 0.005f;
        mCtlAlpha = 1.0f - std::exp(-1.0f / (tau * mSR));

        // Envelope follower: fast attack, slower release feel (single alpha approx)
        // ~15ms-ish
        const float etau = 0.015f;
        mEnvAlpha = 1.0f - std::exp(-1.0f / (etau * mSR));
    }

private:
    float mSR = 48000.0f;

    // User controls
    float mDrive = 1.0f;
    float mSize  = 0.6f;
    float mIron  = 0.5f;
    float mLoad  = 0.5f;
    float mMix   = 1.0f;

    float mLFCorner = 180.0f;
    float mPhase    = 0.25f;
    float mDCReject = 0.0f;

    // Smoothed controls
    float mDriveLP = 1.0f;
    float mSizeLP  = 0.6f;
    float mIronLP  = 0.5f;
    float mLoadLP  = 0.5f;
    float mMixLP   = 1.0f;

    float mIronCached = -1.0f;

    // Split
    float mSplitG = 0.01f;
    float mLP     = 0.0f;

    // Core state
    float mFlux = 0.0f;
    float mHys  = 0.0f;
    float mEnv  = 0.0f;

    // Memory coeffs
    float mFluxLeak  = 0.999f;
    float mHysLeak   = 0.9995f;
    float mFluxDrive = 1.0f;

    // Smoothing/envelope coeffs
    float mCtlAlpha = 0.02f;
    float mEnvAlpha = 0.01f;

    // Phase smear (allpass)
    float mAP_a1 = 0.15f;
    float mAP_a2 = 0.10f;
    float mAPz1  = 0.0f;
    float mAPz2  = 0.0f;

    // Optional DC reject state
    float mDCZ = 0.0f;
};
