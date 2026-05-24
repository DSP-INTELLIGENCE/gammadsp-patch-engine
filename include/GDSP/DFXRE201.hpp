// RE201.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include "Delay.hpp"

// ------------------------------------------------------------
// Small helpers (keep lightweight / inlineable)
// ------------------------------------------------------------
static inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

struct OnePoleHP {
    // Simple DC/rumble remover (preamp coupling cap feel)
    void set(float cutoffHz, float sr) {
        // bilinear transform for 1st order HPF
        float x = std::exp(-2.0f * float(M_PI) * cutoffHz / sr);
        a = x;
        b = (1.0f + x) * 0.5f;
    }
    float process(float in) {
        float y = b * (in - x1) + a * y1;
        x1 = in; y1 = y;
        return y;
    }
    float a = 0.0f, b = 1.0f;
    float x1 = 0.0f, y1 = 0.0f;
};

// From earlier messages (or place in their own headers)
class TapeDamp {
public:
    void set(float cutoff, float sr) {
        float x = std::exp(-2.0f * float(M_PI) * cutoff / sr);
        a = x; b = 1.0f - x;
    }
    float process(float x) { z = b * x + a * z; return z; }
private:
    float a = 0.0f, b = 1.0f, z = 0.0f;
};

class TapeSat {
public:
    void setDrive(float d) { drive = std::max(0.0001f, d); norm = 1.0f / std::tanh(drive); }
    float process(float x) { return std::tanh(x * drive) * norm; }
private:
    float drive = 2.0f;
    float norm  = 1.0f / std::tanh(2.0f);
};

class SoftComp {
public:
    void set(float threshold, float ratio) {
        th = std::max(0.0001f, threshold);
        invRatio = 1.0f / std::max(1.0f, ratio);
    }
    float process(float x) {
        float a = std::fabs(x);
        if (a <= th) return x;
        float excess = (a - th) * invRatio;
        float y = th + excess;
        return std::copysign(y, x);
    }
private:
    float th = 0.35f;
    float invRatio = 0.4f; // ratio 2.5:1
};

class TapeMod {
public:
    explicit TapeMod(float sr = 48000.f) : sampleRate(sr) {}

    void setWow(float depthSeconds, float rateHz)     { wowDepth = depthSeconds; wowRate = rateHz; }
    void setFlutter(float depthSeconds, float rateHz) { flutterDepth = depthSeconds; flutterRate = rateHz; }
    void setJitter(float depthSeconds)                { jitterDepth = depthSeconds; }

    float process() {
        wowPhase     += wowRate     * twoPi / sampleRate;
        flutterPhase += flutterRate * twoPi / sampleRate;
        // fixed-ish micro wobble (can be parameterized)
        jitterPhase  += 31.7f       * twoPi / sampleRate;
        wrap(wowPhase); wrap(flutterPhase); wrap(jitterPhase);

        float wow     = std::sin(wowPhase)     * wowDepth;
        float flutter = std::sin(flutterPhase) * flutterDepth;

        float jitter  = ((float(std::rand()) / float(RAND_MAX)) * 2.f - 1.f) * jitterDepth;
        return wow + flutter + jitter;
    }

private:
    float sampleRate = 48000.f;
    float wowDepth = 0.0006f, flutterDepth = 0.00018f, jitterDepth = 0.00003f;
    float wowRate = 0.28f, flutterRate = 6.2f;
    float wowPhase = 0.f, flutterPhase = 0.f, jitterPhase = 0.f;
    const float twoPi = 6.28318530718f;

    void wrap(float& p) { if (p > twoPi) p -= twoPi; }
};

// ------------------------------------------------------------
// RE-201 Skeleton
// ------------------------------------------------------------
class DFXRE201 : public Function {
public:
    enum class Mode : int {
        H1 = 1,
        H2,
        H3,
        H4,
        H1_H2,
        H1_H3,
        H2_H3,
        H1_H2_H3,
        H1_H2_H3_H4,
        REVERB_ONLY,
        ECHO_REVERB
    };

    explicit DFXRE201(float sampleRate = 48000.f)
    : mSR(sampleRate)
    , mHeads(0.6f, 4, 0.1f) // max 600ms, 4 taps, init 100ms
    , mReverb()            // you can swap this to PRCRev / DattorroVerb later
    , mTapeMod(sampleRate)
    {
        // Interpolation / character
        mHeads.setIpolType(LINEAR); // try CUBIC for smoother warble
        // Preamp
        mHP.set(35.0f, mSR);

        // Default head times (rough Space Echo-ish spacing; refined later)
        // (These are baseline "tape speed" times; Rate knob scales around them)
        mBaseHeadTimes[0] = 0.065f;  // 65 ms
        mBaseHeadTimes[1] = 0.110f;  // 110 ms
        mBaseHeadTimes[2] = 0.170f;  // 170 ms
        mBaseHeadTimes[3] = 0.240f;  // 240 ms

        // Default head levels (tweak to taste)
        mHeadGains[0] = 1.00f;
        mHeadGains[1] = 0.90f;
        mHeadGains[2] = 0.85f;
        mHeadGains[3] = 0.80f;

        // Tape loop defaults
        mDamp.set(9000.f, mSR);
        mSat.setDrive(2.5f);
        mComp.set(0.35f, 2.5f);
        mTapeMod.setWow(0.0006f, 0.28f);
        mTapeMod.setFlutter(0.00018f, 6.2f);
        mTapeMod.setJitter(0.00003f);

        setMode(Mode::H1_H2_H3);
        updateHeadEnables();
        updateHeadTimes();
    }

    // --------------------------------------------------------
    // Main controls (front-panel style)
    // --------------------------------------------------------
    void setMode(Mode m) { mMode = m; updateHeadEnables(); }

    // "Repeat Rate" on RE-201 (controls tape speed -> scales head times)
    // Expect 0..1; map to ~0.6x .. 1.6x speed (tweak as needed)
    void setRepeatRate(float rate01) {
        mRate = clamp01(rate01);
        updateHeadTimes();
    }

    // "Intensity" (feedback)
    void setIntensity(float intensity01) {
        // allow self-oscillation but keep safety margin
        mFeedback = std::clamp(intensity01, 0.0f, 0.95f);
    }

    // "Echo Volume" (echo wet)
    void setEchoLevel(float echo01) { mEchoLevel = clamp01(echo01); }

    // "Reverb Volume"
    void setReverbLevel(float rev01) { mReverbLevel = clamp01(rev01); }

    // Output mix between dry and effect (optional global wet)
    void setWet(float wet01) { mWet = clamp01(wet01); }

    // Preamp drive
    void setInputDrive(float drive) { mInputDrive = std::max(0.0f, drive); }

    // Tape character macro (0=new tape, 1=old tape)
    void setTapeAge(float age01) {
        mTapeAge = clamp01(age01);

        // More age -> more drive, more HF loss, more modulation
        float drive = 2.0f + 3.0f * mTapeAge;
        mSat.setDrive(drive);

        float cutoff = 11000.f - 6500.f * mTapeAge;   // 11k -> 4.5k
        mDamp.set(std::max(1500.f, cutoff), mSR);

        float wowD     = 0.00035f + 0.00055f * mTapeAge;
        float flutterD = 0.00010f + 0.00020f * mTapeAge;
        float jitterD  = 0.000015f + 0.00005f * mTapeAge;
        mTapeMod.setWow(wowD, 0.22f + 0.20f * mTapeAge);
        mTapeMod.setFlutter(flutterD, 5.5f + 2.0f * mTapeAge);
        mTapeMod.setJitter(jitterD);

        // Slightly more compression as tape gets older
        mComp.set(0.38f - 0.10f * mTapeAge, 2.0f + 1.5f * mTapeAge);
    }

    // Optional fine controls (if you want)
    void setWowFlutter(float wowDepthSec, float wowRateHz,
                       float flutterDepthSec, float flutterRateHz,
                       float jitterDepthSec)
    {
        mTapeMod.setWow(wowDepthSec, wowRateHz);
        mTapeMod.setFlutter(flutterDepthSec, flutterRateHz);
        mTapeMod.setJitter(jitterDepthSec);
    }

    // --------------------------------------------------------
    // Processing
    // --------------------------------------------------------
    float process(float x) override
    {
        // --- Preamp ---
        float in = mHP.process(x);
        in = std::tanh(in * (1.0f + mInputDrive));

        // --- Wow / Flutter (true time modulation) ---
        float wob = mTapeMod.process();                 // seconds
        mWobSm += (wob - mWobSm) * 0.0025f;             // smooth
        updateHeadTimesWithWob(mWobSm);                 // <- real pitch modulation

        // --- Read heads ---
        float h[4] = {
            mHeads.read(0),
            mHeads.read(1),
            mHeads.read(2),
            mHeads.read(3),
        };

        // --- Mix enabled heads ---
        float echoOut = 0.f;
        float norm = 0.f;
        for (int i = 0; i < 4; ++i)
            if (mHeadEnable[i]) {
                echoOut += h[i] * mHeadGains[i];
                norm    += mHeadGains[i];
            }
        if (norm > 0.f) echoOut /= norm;

        // --- Tape color on audible echo (important) ---
        float tapeEcho = mDamp.process(
                        mComp.process(
                        mSat.process(echoOut)));

        // --- Feedback ---
        float fb = tapeEcho * mFeedback;

        // --- Write tape ---
        mHeads.write(in + fb);

        // --- Reverb (single call, correct) ---
        float revOut = mReverb.process(in + tapeEcho * 0.35f);

        // --- Output mix ---
        float wetSig = tapeEcho * mEchoLevel + revOut * mReverbLevel;
        return in * (1.0f - mWet) + wetSig * mWet;
    }


    void run(const float* input, float* output, size_t n) {
        for (size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

private:
    // --------------------------------------------------------
    // Internal updates
    // --------------------------------------------------------
    void updateHeadEnables()
    {
        std::fill(std::begin(mHeadEnable), std::end(mHeadEnable), false);

        switch (mMode) {
            case Mode::H1:               mHeadEnable[0] = true; break;
            case Mode::H2:               mHeadEnable[1] = true; break;
            case Mode::H3:               mHeadEnable[2] = true; break;
            case Mode::H4:               mHeadEnable[3] = true; break;
            case Mode::H1_H2:            mHeadEnable[0] = mHeadEnable[1] = true; break;
            case Mode::H1_H3:            mHeadEnable[0] = mHeadEnable[2] = true; break;
            case Mode::H2_H3:            mHeadEnable[1] = mHeadEnable[2] = true; break;
            case Mode::H1_H2_H3:         mHeadEnable[0] = mHeadEnable[1] = mHeadEnable[2] = true; break;
            case Mode::H1_H2_H3_H4:      mHeadEnable[0] = mHeadEnable[1] = mHeadEnable[2] = mHeadEnable[3] = true; break;
            case Mode::REVERB_ONLY:      /* no heads */ break;
            case Mode::ECHO_REVERB:      mHeadEnable[0] = mHeadEnable[1] = mHeadEnable[2] = true; break;
        }
    }

    void updateHeadTimes()
    {
        // Map rate knob to a time scale factor (tape speed).
        // Higher "rate" -> shorter times (faster tape). Choose mapping by ear.
        float speed = 0.6f + 1.4f * mRate; // 0.6..2.0
        float scale = 1.0f / speed;

        for (int i = 0; i < 4; ++i) {
            float t = mBaseHeadTimes[i] * scale;
            // clamp to max delay
            t = std::clamp(t, 0.001f, 0.59f);
            mHeads.setTapTime((unsigned)i, t);
        }
    }
    void updateHeadTimesWithWob(float wobSec)
    {
        float speed = 0.6f + 1.4f * mRate;
        float scale = 1.f / speed;

        for (int i = 0; i < 4; ++i) {
            float t = mBaseHeadTimes[i] * scale + wobSec;
            t = std::clamp(t, 0.001f, 0.59f);
            mHeads.delay(t, i);   // <-- Gamma’s real fractional modulation
        }
    }

private:
    float mSR = 48000.f;

    // Core delay (multi-head)
    MultitapDelay mHeads;

    // Reverb (swap out if you prefer PRCRev/SMReverb/DattorroVerb)
    FreeVerb mReverb;

    // Tape loop processing
    TapeDamp  mDamp;
    TapeSat   mSat;
    SoftComp  mComp;
    TapeMod   mTapeMod;

    // Preamp conditioning
    OnePoleHP mHP;

    // State / params
    Mode  mMode = Mode::H1_H2_H3;

    float mRate       = 0.5f;  // 0..1
    float mFeedback   = 0.55f; // 0..0.95
    float mEchoLevel  = 0.7f;  // 0..1
    float mReverbLevel= 0.35f; // 0..1
    float mWet        = 1.0f;  // 0..1
    float mInputDrive = 0.5f;  // arbitrary
    float mTapeAge    = 0.25f; // 0..1

    // Heads
    float mBaseHeadTimes[4] = {0.065f, 0.110f, 0.170f, 0.240f};
    float mHeadGains[4]     = {1.0f, 0.9f, 0.85f, 0.8f};
    bool  mHeadEnable[4]    = {true, true, true, false};
};
