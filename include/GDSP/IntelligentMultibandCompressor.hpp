// IntelligentMultibandCompressor.hpp
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// You already built these (or close equivalents):
#include "ThresholdComparator.hpp"
#include "GainController.hpp"
#include "CharacterEngine.hpp"
#include "LookaheadBuffer.hpp"

// ============================================================
// 1) Minimal DSP primitives: Biquad + Linkwitz-Riley crossovers
// ============================================================

template <class Sample>
struct Biquad {
    // Direct Form I
    Sample b0=1, b1=0, b2=0, a1=0, a2=0;
    Sample z1=0, z2=0;

    void reset() { z1 = z2 = 0; }

    Sample process(Sample x) {
        // Transposed DF2 (stable, minimal state)
        Sample y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;
        return y;
    }

    static Biquad lowpass(Sample fc, Sample q, Sample fs) {
        fc = std::clamp(fc, (Sample)5, (Sample)(0.49*fs));
        q  = std::max((Sample)1e-6, q);
        const Sample w0 = (Sample)2 * (Sample)M_PI * fc / fs;
        const Sample c  = std::cos(w0);
        const Sample s  = std::sin(w0);
        const Sample alpha = s / ((Sample)2 * q);

        Biquad biq;
        Sample b0 = ((Sample)1 - c) / (Sample)2;
        Sample b1 =  (Sample)1 - c;
        Sample b2 = ((Sample)1 - c) / (Sample)2;
        Sample a0 =  (Sample)1 + alpha;
        Sample a1 = -(Sample)2 * c;
        Sample a2 =  (Sample)1 - alpha;

        biq.b0 = b0/a0; biq.b1 = b1/a0; biq.b2 = b2/a0;
        biq.a1 = a1/a0; biq.a2 = a2/a0;
        return biq;
    }

    static Biquad highpass(Sample fc, Sample q, Sample fs) {
        fc = std::clamp(fc, (Sample)5, (Sample)(0.49*fs));
        q  = std::max((Sample)1e-6, q);
        const Sample w0 = (Sample)2 * (Sample)M_PI * fc / fs;
        const Sample c  = std::cos(w0);
        const Sample s  = std::sin(w0);
        const Sample alpha = s / ((Sample)2 * q);

        Biquad biq;
        Sample b0 = ((Sample)1 + c) / (Sample)2;
        Sample b1 = -((Sample)1 + c);
        Sample b2 = ((Sample)1 + c) / (Sample)2;
        Sample a0 =  (Sample)1 + alpha;
        Sample a1 = -(Sample)2 * c;
        Sample a2 =  (Sample)1 - alpha;

        biq.b0 = b0/a0; biq.b1 = b1/a0; biq.b2 = b2/a0;
        biq.a1 = a1/a0; biq.a2 = a2/a0;
        return biq;
    }
};

// Linkwitz-Riley 4th order crossover made from cascaded Butterworth 2nd order (Q = 1/sqrt(2)).
template <class Sample>
struct LR4Crossover {
    Biquad<Sample> lp1, lp2, hp1, hp2;
    Sample fc = 1000;

    void set(Sample cutoffHz, Sample fs) {
        fc = cutoffHz;
        const Sample q = (Sample)(1.0 / std::sqrt(2.0)); // Butterworth
        lp1 = Biquad<Sample>::lowpass (fc, q, fs);
        lp2 = Biquad<Sample>::lowpass (fc, q, fs);
        hp1 = Biquad<Sample>::highpass(fc, q, fs);
        hp2 = Biquad<Sample>::highpass(fc, q, fs);
    }

    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }

    // Split x into low/high with phase-aligned LR4 responses
    inline void process(Sample x, Sample& low, Sample& high) {
        low  = lp2.process(lp1.process(x));
        high = hp2.process(hp1.process(x));
    }
};

// ==============================================
// 2) Frequency-dependent program analysis (per band)
// ==============================================

template <class Sample>
struct BandFeatures {
    Sample peak = 0;       // peak-ish envelope (linear)
    Sample rms  = 0;       // energy envelope (linear)
    Sample crest = 0;      // peak/rms (>=1)
    Sample density = 0;    // rms/peak (0..1)
    Sample transient = 0;  // |drms/dt|
    Sample energyDb = 0;   // 20log10(rms)
};

// Very light-weight per-band feature extractor.
// Uses the CharacterEngine *as the envelope source* (already includes curvature/nonlinearity),
// plus a simple derivative to estimate transientness.
//
// If you prefer separate peak/rms, swap in your PeakDetector + ExpRMSFollower here.
template <class Sample>
class BandProgramAnalyzer {
public:
    void setTimes(Sample atk, Sample rel) { mEnv.setAttack(atk); mEnv.setRelease(rel); }
    void setCharacterMorph(Sample m)      { mEnv.setMorph(m); } // optional
    void reset() { mEnv.reset(); mPrevRms = 0; mFeat = {}; }

    // Input should be the band-limited signal.
    const BandFeatures<Sample>& process(Sample x) {
        // Using character engine as a “smart detector”: output is an envelope (linear).
        // For analysis we want both peak-ish and rms-ish estimates; we approximate:
        // - peak: faster settings
        // - rms:  slower settings
        //
        // To keep it cheap, we run TWO envelopes: fast and slow.
        Sample fast = mFast.process(x);
        Sample slow = mSlow.process(x);

        mFeat.peak = fast;
        mFeat.rms  = slow;

        const Sample eps = (Sample)1e-12;
        mFeat.crest   = (mFeat.peak + eps) / (mFeat.rms + eps);
        mFeat.density = (mFeat.rms + eps) / (mFeat.peak + eps);

        Sample dr = mFeat.rms - mPrevRms;
        mFeat.transient = std::abs(dr);
        mPrevRms = mFeat.rms;

        mFeat.energyDb = (Sample)20 * std::log10(std::max(mFeat.rms, eps));
        return mFeat;
    }

    const BandFeatures<Sample>& features() const { return mFeat; }

    // Configure fast/slow detector personalities
    void setFastTimes(Sample atk, Sample rel) { mFast.setAttack(atk); mFast.setRelease(rel); }
    void setSlowTimes(Sample atk, Sample rel) { mSlow.setAttack(atk); mSlow.setRelease(rel); }
    void setFastMorph(Sample m) { mFast.setMorph(m); }
    void setSlowMorph(Sample m) { mSlow.setMorph(m); }

private:
    CharacterEngine<Sample> mEnv;   // unused in this minimal version, kept if you want single-detector mode
    CharacterEngine<Sample> mFast;  // “peak-ish”
    CharacterEngine<Sample> mSlow;  // “rms-ish”
    Sample mPrevRms = 0;
    BandFeatures<Sample> mFeat{};
};

// ============================================================
// 3) Per-band intelligent compressor: analysis → adapt → compress
// ============================================================

template <class Sample>
class IntelligentBandCompressor {
public:
    void setSampleRate(Sample fs) { mFs = fs; }

    // User “static” params
    void setThresholdDb(Sample t) { mCurve.setThreshold(t); }
    void setKneeDb(Sample k)      { mCurve.setKnee(std::max((Sample)0, k)); }
    void setRatio(Sample r)       { mRatio = std::max((Sample)1, r); }
    void setMakeupDb(Sample db)   { mMakeup = std::pow((Sample)10, db/(Sample)20); }
    void setMode(Mode m)          { mMode = m; mCurve.setMode(m); }

    // Base timing (adaptive system will bend these)
    void setAttack(Sample s)  { mBaseAtk = std::max((Sample)1e-6, s); }
    void setRelease(Sample s) { mBaseRel = std::max((Sample)1e-6, s); }
    void setHold(Sample s)    { mHoldSec = std::max((Sample)0, s); mGain.setHold(s); }

    // Character endpoints for this band (low/mid/high often differ)
    void setCharacterRange(Sample morphMin01, Sample morphMax01) {
        mCharMin = std::clamp(morphMin01, (Sample)0, (Sample)1);
        mCharMax = std::clamp(morphMax01, (Sample)0, (Sample)1);
    }

    // “Intelligence” amount 0..1
    void setIntelligence(Sample a01) { mIntel = std::clamp(a01, (Sample)0, (Sample)1); }

    void reset() {
        mGain.reset((Sample)0);
        mDetector.reset();
        mEnv.reset();
        mLastTargetDb = 0;
        mLastGainDb = 0;
        mGrDb = 0;
    }

    // Process one sample of band-limited audio
    Sample process(Sample xBand)
    {
        // --- Frequency-dependent program analysis (per band)
        const auto& f = mDetector.process(xBand);

        // --- Derive adaptive detector “character” and time constants from features
        // Normalize features into [0..1] controls (very practical mapping)
        Sample transient01 = std::clamp(f.transient * (Sample)50, (Sample)0, (Sample)1); // tune scalar
        Sample dense01     = std::clamp((Sample)1 - f.crest / (Sample)6, (Sample)0, (Sample)1); // crest 1..6 typical
        Sample energy01    = std::clamp((f.energyDb + (Sample)60) / (Sample)60, (Sample)0, (Sample)1);

        // Character: more transient → FET-ish; more dense → VCA/Opto; high energy → Tube smoothing
        Sample charT = (Sample)0.50 * transient01 + (Sample)0.25 * dense01 + (Sample)0.25 * energy01;
        Sample morph = lerp(mCharMin, mCharMax, charT);

        // Timing: transient → faster attack; dense/energy → slower release (glue)
        Sample atk = mBaseAtk * lerp((Sample)1.6, (Sample)0.35, transient01);  // 1.6x slower to 0.35x faster
        Sample rel = mBaseRel * lerp((Sample)0.7, (Sample)4.0, dense01);       // faster to much slower

        // Apply intelligence amount (blend between fixed and adaptive)
        atk = lerp(mBaseAtk, atk, mIntel);
        rel = lerp(mBaseRel, rel, mIntel);

        // Drive the detector used for control (CharacterEngine)
        mEnv.setMorph(morph);
        mEnv.setAttack(atk);
        mEnv.setRelease(rel);

        // Detector envelope (linear)
        Sample env = mEnv.process(xBand);

        // --- Transfer function → target gain (dB), then smooth GAIN (GainController)
        env = std::max(env, (Sample)1e-12);
        Sample envDb = (Sample)20 * std::log10(env);

        // Comparator returns “how far into action” (your earlier design)
        Sample cDb = mCurve.process(env);

        Sample targetGainDb = 0;

        if (mMode == Gate) {
            targetGainDb = -std::max((Sample)0, cDb);
        } else if (mMode == Upward) {
            // cDb is negative (your comparator), treat magnitude like a ratio scaling and boost
            Sample mag = std::abs(cDb);
            Sample scaled = mag * ((Sample)1 - (Sample)1 / mRatio);
            targetGainDb = +scaled;
        } else { // Downward
            Sample grDb = std::max((Sample)0, cDb) * ((Sample)1 - (Sample)1 / mRatio);
            targetGainDb = -grDb;
        }

        // Smooth the gain (attack/release handled inside GainController)
        mGain.setAttack(atk);
        mGain.setRelease(rel);
        mGain.setHold(mHoldSec);

        Sample smoothGainDb = mGain.process(targetGainDb);
        mLastTargetDb = targetGainDb;
        mLastGainDb = smoothGainDb;
        mGrDb = std::max((Sample)0, -smoothGainDb);

        Sample gLin = std::pow((Sample)10, smoothGainDb / (Sample)20);
        return xBand * gLin * mMakeup;
    }

    // meters
    Sample gainDb() const { return mLastGainDb; }
    Sample targetDb() const { return mLastTargetDb; }
    Sample gainReductionDb() const { return mGrDb; }
    const BandFeatures<Sample>& features() const { return mDetector.features(); }

private:
    static inline Sample lerp(Sample a, Sample b, Sample t) { return a + (b - a) * std::clamp(t, (Sample)0, (Sample)1); }

    Sample mFs = 48000;

    // Static curve
    ThresholdComparator<Sample> mCurve;
    Mode  mMode = Downward;
    Sample mRatio = (Sample)4;

    // “Control detector” and “analysis detector”
    CharacterEngine<Sample>       mEnv;
    BandProgramAnalyzer<Sample>   mDetector;

    // Gain smoothing
    GainController<Sample> mGain;

    // Parameters
    Sample mBaseAtk = (Sample)0.010;
    Sample mBaseRel = (Sample)0.120;
    Sample mHoldSec = (Sample)0.0;
    Sample mMakeup  = (Sample)1.0;

    Sample mIntel   = (Sample)0.7;  // 0..1
    Sample mCharMin = (Sample)0.10; // “clean-ish”
    Sample mCharMax = (Sample)0.90; // “tube-ish”

    // meters
    Sample mLastTargetDb = 0;
    Sample mLastGainDb = 0;
    Sample mGrDb = 0;
};

// ============================================================
// 4) Full intelligent multiband compressor (3-band LR4 by default)
// ============================================================

template <class Sample>
class IntelligentMultibandCompressor {
public:
    explicit IntelligentMultibandCompressor(int maxLookaheadSamples = 4096)
    : mLookahead(maxLookaheadSamples)
    {}

    void setSampleRate(Sample fs) {
        mFs = fs;
        mX1.set(mFc1, mFs);
        mX2.set(mFc2, mFs);
        for (auto& b : mBand) b.setSampleRate(fs);
        updateLookahead();
    }

    // 3-band crossover points
    void setCrossover1(Sample hz) { mFc1 = hz; mX1.set(mFc1, mFs); }
    void setCrossover2(Sample hz) { mFc2 = hz; mX2.set(mFc2, mFs); }

    // Lookahead for the AUDIO path (samples). Control path is “instant”.
    void setLookaheadSamples(int s) { mLookaheadSamples = s; updateLookahead(); }

    // Global “intelligence” 0..1
    void setIntelligence(Sample a01) { for (auto& b : mBand) b.setIntelligence(a01); }

    // Per-band parameters (0=Low,1=Mid,2=High)
    IntelligentBandCompressor<Sample>& band(int i) { return mBand[std::clamp(i,0,2)]; }
    const IntelligentBandCompressor<Sample>& band(int i) const { return mBand[std::clamp(i,0,2)]; }

    void reset() {
        mX1.reset(); mX2.reset();
        mLookahead.reset();
        for (auto& b : mBand) b.reset();
    }

    // Process mono sample
    Sample process(Sample x)
    {
        // 1) Split into 3 bands with two LR4 crossovers
        Sample low, high1;
        mX1.process(x, low, high1);      // low + (mid+high)

        Sample mid, high;
        mX2.process(high1, mid, high);   // mid + high

        // 2) Process each band with intelligent control
        Sample lowP  = mBand[0].process(low);
        Sample midP  = mBand[1].process(mid);
        Sample highP = mBand[2].process(high);

        // 3) Lookahead audio alignment (optional):
        // In a “true” lookahead MB comp, you delay the entire audio path equally.
        // Here we delay input and then *replace* x with delayed x for dry/wet logic if desired.
        // For pure multiband sum, this delay is not strictly required, but it enables brickwall-ish behavior.
        Sample xDelayed = (mLookaheadSamples > 0) ? mLookahead.process(x) : x;

        // 4) Recombine processed bands (LR4 sums flat)
        // If lookahead is active, you'd typically apply it pre-split; for simplicity we leave it here.
        // In production: delay x before split so band alignment is perfect through crossovers.
        (void)xDelayed; // keep if you later add dry/wet or a limiter stage
        return lowP + midP + highP;
    }

    void processBuffer(const Sample* in, Sample* out, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) out[i] = process(in[i]);
    }

private:
    void updateLookahead() { mLookahead.setDelay(mLookaheadSamples); }

    Sample mFs = 48000;
    Sample mFc1 = (Sample)120;   // low/mid split
    Sample mFc2 = (Sample)2500;  // mid/high split

    LR4Crossover<Sample> mX1;
    LR4Crossover<Sample> mX2;

    std::array<IntelligentBandCompressor<Sample>, 3> mBand;

    LookaheadBuffer<Sample> mLookahead;
    int mLookaheadSamples = 0;
};
```

## How to set it up (example)

```cpp
IntelligentMultibandCompressor<float> mb(4096);
mb.setSampleRate(48000);
mb.setCrossover1(120.0f);
mb.setCrossover2(2500.0f);
mb.setLookaheadSamples(128);
mb.setIntelligence(0.75f);

// Low band: slower, smoother
mb.band(0).setThresholdDb(-28);
mb.band(0).setRatio(2.5f);
mb.band(0).setAttack(0.020f);
mb.band(0).setRelease(0.200f);
mb.band(0).setCharacterRange(0.20f, 0.80f); // VCA→Opto/Tubey

// Mid band: general glue
mb.band(1).setThresholdDb(-24);
mb.band(1).setRatio(3.0f);
mb.band(1).setAttack(0.010f);
mb.band(1).setRelease(0.120f);
mb.band(1).setCharacterRange(0.10f, 0.70f); // Clean→Opto

// High band: faster transient control / de-ess behavior
mb.band(2).setThresholdDb(-30);
mb.band(2).setRatio(4.0f);
mb.band(2).setAttack(0.003f);
mb.band(2).setRelease(0.060f);
mb.band(2).setCharacterRange(0.35f, 0.65f); // VCA→FET
