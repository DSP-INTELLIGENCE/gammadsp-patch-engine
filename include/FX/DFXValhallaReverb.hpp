// ValhallaReverb.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include <memory>
#include "Delay.hpp"

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
static inline float clampf(float x, float a, float b) { return std::clamp(x, a, b); }

// 1-pole LP (HF damping) -- similar to your TapeDamp
class OnePoleLP {
public:
    void setCutoff(float hz, float sr) {
        hz = std::max(10.0f, hz);
        float x = std::exp(-2.0f * float(M_PI) * hz / sr);
        a = x; b = 1.0f - x;
    }
    float process(float x) { z = b * x + a * z; return z; }
private:
    float a = 0.0f, b = 1.0f, z = 0.0f;
};

// 1-pole HP (LF damping / low-cut in the tank)
class OnePoleHP {
public:
    void setCutoff(float hz, float sr) {
        hz = std::max(10.0f, hz);
        float x = std::exp(-2.0f * float(M_PI) * hz / sr);
        a = x;
        b = (1.0f + x) * 0.5f;
    }
    float process(float x) {
        float y = b * (x - x1) + a * y1;
        x1 = x; y1 = y;
        return y;
    }
private:
    float a = 0.0f, b = 1.0f;
    float x1 = 0.0f, y1 = 0.0f;
};

// Soft saturation for feedback nonlinearity (very light)
class SoftSat {
public:
    void setDrive(float d) {
        drive = std::max(0.0001f, d);
        norm  = 1.0f / std::tanh(drive);
    }
    float process(float x) const {
        return std::tanh(x * drive) * norm;
    }
private:
    float drive = 1.5f;
    float norm  = 1.0f / std::tanh(1.5f);
};

// Simple sine LFO
struct SineLFO {
    void set(float rateHz, float sr) { rate = rateHz; invSR = 1.0f / sr; }
    float process() {
        phase += 2.0f * float(M_PI) * rate * invSR;
        if (phase > 2.0f * float(M_PI)) phase -= 2.0f * float(M_PI);
        return std::sin(phase);
    }
    float rate = 0.25f;
    float invSR = 1.0f / 48000.f;
    float phase = 0.0f;
};

// ------------------------------------------------------------
// Valhalla-ish Reverb Tank (dual tanks with cross-feedback)
// Uses Comb in allpass mode as diffusers, and Delay lines in tank
// ------------------------------------------------------------
class ValhallaReverb : public Function {
public:
    enum class Mode {
        ROOM,
        HALL,
        PLATE,
        CHAMBER,
        DARK,
        BRIGHT,
        VINTAGE
    };

    explicit ValhallaReverb(float sampleRate = 48000.f)
    : mSR(sampleRate),
      mPreDelay(0.25f, 0.03f)
    {
        // --- Input diffusion (series allpass) ---
        for (int i = 0; i < kInDiff; ++i) {
            mInAP[i] = std::make_unique<Comb>(0.05f);
            mInAP[i]->setAllPass(0.75f);
            mInAP[i]->setIpolType(LINEAR);
        }
        // Good starting diffusion times (tweak per mode/size)
        mInAPTimes[0] = 0.007f;
        mInAPTimes[1] = 0.011f;
        mInAPTimes[2] = 0.015f;
        mInAPTimes[3] = 0.019f;
        for (int i = 0; i < kInDiff; ++i) mInAP[i]->setDelay(mInAPTimes[i]);

        // --- Pre-delay ---
        mPreDelay.setIpolType(LINEAR);
        setPreDelayMs(30.0f);

        // --- Tank delay lines (L/R, 4 per side) ---
        for (int i = 0; i < kTank; ++i) {
            mDL[i] = std::make_unique<Delay>(0.5f, 0.09f);
            mDR[i] = std::make_unique<Delay>(0.5f, 0.09f);
            mDL[i]->setIpolType(CUBIC);
            mDR[i]->setIpolType(CUBIC);
        }

        // Base times (seconds) for a hall-ish start; "size" scales these
        mTankBaseL[0] = 0.060f; mTankBaseL[1] = 0.089f; mTankBaseL[2] = 0.073f; mTankBaseL[3] = 0.097f;
        mTankBaseR[0] = 0.067f; mTankBaseR[1] = 0.083f; mTankBaseR[2] = 0.079f; mTankBaseR[3] = 0.101f;

        // --- Tank diffusion (optional, light) ---
        for (int i = 0; i < kLateDiff; ++i) {
            mLateAPL[i] = std::make_unique<Comb>(0.1f);
            mLateAPR[i] = std::make_unique<Comb>(0.1f);
            mLateAPL[i]->setAllPass(0.6f);
            mLateAPR[i]->setAllPass(0.6f);
            mLateAPL[i]->setIpolType(LINEAR);
            mLateAPR[i]->setIpolType(LINEAR);
        }
        mLateTimes[0] = 0.0063f;
        mLateTimes[1] = 0.0041f;
        for (int i = 0; i < kLateDiff; ++i) {
            mLateAPL[i]->setDelay(mLateTimes[i]);
            mLateAPR[i]->setDelay(mLateTimes[i] * 1.07f);
        }

        // --- Filters / nonlinearity ---
        mHFL.setCutoff(8000.0f, mSR);
        mHFR.setCutoff(8000.0f, mSR);
        mLFL.setCutoff(250.0f,  mSR);
        mLFR.setCutoff(250.0f,  mSR);
        mSat.setDrive(1.6f);

        // --- Modulation ---
        mLfoA.set(0.25f, mSR);
        mLfoB.set(0.37f, mSR);
        setModDepthMs(0.6f);

        // --- Defaults ---
        setMode(Mode::HALL);
        setSize(0.75f);
        setDecay(0.85f);
        setCross(0.35f);
        setMix(0.35f);
        setColor(0.5f);

        updateDelays();
        updateVoicing();
    }

    // --------------------------------------------------------
    // User-facing controls (Valhalla-ish)
    // --------------------------------------------------------
    void setMode(Mode m) { mMode = m; updateVoicing(); }

    // 0..1 scales internal delay lengths
    void setSize(float size01) { mSize = clamp01(size01); updateDelays(); }

    // 0..0.99
    void setDecay(float decay01) { mDecay = clampf(decay01, 0.05f, 0.99f); }

    void setPreDelayMs(float ms) {
        float s = std::max(0.0f, ms) * 0.001f;
        mPreDelay.delay(s);
    }

    void setMix(float mix01) { mMix = clamp01(mix01); }

    // cross-feedback amount between L/R tanks
    void setCross(float cross01) { mCross = clamp01(cross01); }

    void setModRate(float hzA, float hzB) {
        mLfoA.set(std::max(0.01f, hzA), mSR);
        mLfoB.set(std::max(0.01f, hzB), mSR);
    }

    void setModDepthMs(float ms) { mModDepth = std::max(0.0f, ms) * 0.001f; }

    // "Color" macro: saturation + damping tilt (0=clean/bright, 1=dark/vintage)
    void setColor(float c01) { mColor = clamp01(c01); updateVoicing(); }

    // Output API: process mono in, mono out (engine can expand later to stereo)
    float process(float x) override
    {
        // Input diffusion
        float y = x;
        for (int i = 0; i < kInDiff; ++i) y = mInAP[i]->process(y);

        // Pre-delay
        y = mPreDelay(y);

        // LFOs (two slow modulators)
        float mA = mLfoA.process();
        float mB = mLfoB.process();

        // Mod offsets (seconds) for tank delays (small!)
        float modL = mA * mModDepth;
        float modR = mB * mModDepth;

        // Read tank taps
        float lTap[kTank], rTap[kTank];
        for (int i = 0; i < kTank; ++i) {
            lTap[i] = mDL[i]->read();
            rTap[i] = mDR[i]->read();
        }

        // Sum tank outputs (for feedback)
        float lSum = 0.0f, rSum = 0.0f;
        for (int i = 0; i < kTank; ++i) { lSum += lTap[i]; rSum += rTap[i]; }
        lSum *= (1.0f / float(kTank));
        rSum *= (1.0f / float(kTank));

        // Cross-feedback
        float fbL = lSum * (1.0f - mCross) + rSum * mCross;
        float fbR = rSum * (1.0f - mCross) + lSum * mCross;

        // Frequency-dependent decay inside feedback
        fbL = mLFL.process(fbL);
        fbR = mLFR.process(fbR);
        fbL = mHFL.process(fbL);
        fbR = mHFR.process(fbR);

        // Subtle nonlinearity (vintage “glue”)
        fbL = mSat.process(fbL);
        fbR = mSat.process(fbR);

        fbL *= mDecay;
        fbR *= mDecay;

        // Write tank inputs (shared input + feedback)
        // For a richer network, stagger injection across lines.
        for (int i = 0; i < kTank; ++i) {
            float tL = mTankCurL[i] + ((i & 1) ? modL : -modL);
            float tR = mTankCurR[i] + ((i & 1) ? modR : -modR);

            tL = std::max(0.001f, tL);
            tR = std::max(0.001f, tR);

            // Only update delay time if changed enough (optional micro-optimization)
            mDL[i]->setDelay(tL);
            mDR[i]->setDelay(tR);

            // Distribute injection
            float inj = y * mInjGain[i];
            mDL[i]->write(inj + fbL);
            mDR[i]->write(inj + fbR);
        }

        // Late diffusion (light)
        float outL = lSum;
        float outR = rSum;
        for (int i = 0; i < kLateDiff; ++i) {
            outL = mLateAPL[i]->process(outL);
            outR = mLateAPR[i]->process(outR);
        }

        // Stereo -> mono (skeleton). In a full version, expose outL/outR.
        float wet = 0.5f * (outL + outR);

        // Mix
        return x * (1.0f - mMix) + wet * mMix;
    }

    // If you want a stereo API, add:
    // void process(float x, float& outL, float& outR);

private:
    // --------------------------------------------------------
    // Internal tuning
    // --------------------------------------------------------
    void updateDelays()
    {
        // Scale around base times (size=0 => smaller, size=1 => larger)
        // Map size to 0.6..1.8 scale factor (by ear)
        float scale = 0.6f + 1.2f * mSize;

        for (int i = 0; i < kTank; ++i) {
            mTankCurL[i] = mTankBaseL[i] * scale;
            mTankCurR[i] = mTankBaseR[i] * scale;
        }

        // Injection distribution: slight weights helps density
        mInjGain[0] = 0.40f;
        mInjGain[1] = 0.30f;
        mInjGain[2] = 0.20f;
        mInjGain[3] = 0.10f;
    }

    void updateVoicing()
    {
        // Color macro controls damping + sat
        // color 0 => brighter/cleaner
        // color 1 => darker/vintage
        float hf = 11000.f - 7000.f * mColor; // 11k -> 4k
        float lf = 120.f   + 450.f  * mColor; // 120 -> 570
        float drive = 1.2f + 1.8f * mColor;   // 1.2 -> 3.0

        // Mode offsets
        switch (mMode) {
            case Mode::ROOM:
                hf += 2000.f; lf += 50.f;
                setModDepthMs(0.35f);
                break;
            case Mode::PLATE:
                hf += 800.f; lf += 80.f;
                setModDepthMs(0.45f);
                break;
            case Mode::HALL:
                setModDepthMs(0.65f);
                break;
            case Mode::CHAMBER:
                hf += 1200.f;
                setModDepthMs(0.55f);
                break;
            case Mode::DARK:
                hf -= 2500.f;
                setModDepthMs(0.55f);
                break;
            case Mode::BRIGHT:
                hf += 3000.f;
                setModDepthMs(0.55f);
                break;
            case Mode::VINTAGE:
                hf -= 1500.f; lf += 120.f;
                drive += 0.6f;
                setModDepthMs(0.75f);
                break;
        }

        hf = clampf(hf, 1500.f, 16000.f);
        lf = clampf(lf,  60.f,  900.f);

        mHFL.setCutoff(hf, mSR);
        mHFR.setCutoff(hf * 0.98f, mSR);
        mLFL.setCutoff(lf, mSR);
        mLFR.setCutoff(lf * 1.03f, mSR);

        mSat.setDrive(drive);

        // Slight mode-dependent modulation rates (optional)
        if (mMode == Mode::ROOM) setModRate(0.35f, 0.49f);
        else if (mMode == Mode::VINTAGE) setModRate(0.18f, 0.29f);
        else setModRate(0.25f, 0.37f);
    }

private:
    float mSR = 48000.f;

    // Input diffusion
    static constexpr int kInDiff = 4;
    std::unique_ptr<Comb> mInAP[kInDiff];
    float mInAPTimes[kInDiff] = {0};

    // Pre-delay
    gam::Delay<double> mPreDelay;

    // Tank
    static constexpr int kTank = 4;
    std::unique_ptr<Delay> mDL[kTank];
    std::unique_ptr<Delay> mDR[kTank];

    float mTankBaseL[kTank] = {0};
    float mTankBaseR[kTank] = {0};
    float mTankCurL[kTank]  = {0};
    float mTankCurR[kTank]  = {0};

    float mInjGain[kTank] = {0.4f, 0.3f, 0.2f, 0.1f};

    // Late diffusion
    static constexpr int kLateDiff = 2;
    std::unique_ptr<Comb> mLateAPL[kLateDiff];
    std::unique_ptr<Comb> mLateAPR[kLateDiff];
    float mLateTimes[kLateDiff] = {0.0063f, 0.0041f};

    // Filters / nonlinearity
    OnePoleLP mHFL, mHFR;
    OnePoleHP mLFL, mLFR;
    SoftSat   mSat;

    // Modulation
    SineLFO mLfoA, mLfoB;
    float   mModDepth = 0.0006f;

    // User params
    Mode  mMode  = Mode::HALL;
    float mSize  = 0.75f;
    float mDecay = 0.85f;
    float mCross = 0.35f;
    float mMix   = 0.35f;
    float mColor = 0.5f;
};
