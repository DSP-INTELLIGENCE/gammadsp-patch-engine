// CompressorCore.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

// Assumes you already have these from your earlier snippets:
#include "LookaheadBuffer.hpp"
#include "PeakDetector.hpp"
#include "ExpRMSFollower.hpp"
#include "ThresholdComparator.hpp"

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------
template <class Sample>
static inline Sample linToDb(Sample x, Sample floorLin = (Sample)1e-12)
{
    x = std::max(x, floorLin);
    return (Sample)20 * std::log10(x);
}

template <class Sample>
static inline Sample dbToLin(Sample db)
{
    return std::pow((Sample)10, db / (Sample)20);
}

// ------------------------------------------------------------
// Compressor core (mono)
// Feed-forward topology with optional lookahead.
// Sidechain: internal + external blend, optional filter.
// Detector: peak or "exp RMS" (IIR energy integrator).
// Static curve: threshold + soft-knee, then ratio.
// ------------------------------------------------------------
template <class Sample>
class CompressorCore {
public:
    enum class DetectorType { Peak, ExpRMS };

    // maxLookaheadSamples sets max delay-line capacity (no allocations during processing)
    explicit CompressorCore(int maxLookaheadSamples = 4096)
    : mLookahead(maxLookaheadSamples)
    {
        // Reasonable defaults
        setThresholdDb((Sample)-24);
        setKneeDb((Sample)6);
        setRatio((Sample)4);
        setMakeupDb((Sample)0);
        setDryWet((Sample)1);
        setSidechainBlend((Sample)0);
        setLookaheadSamples(0);
        setDetectorType(DetectorType::ExpRMS);
        setMode(Downward);

        // Default detector times
        mPeak.setAttack((Sample)0.001);
        mPeak.setRelease((Sample)0.050);

        mRms.setAttack((Sample)0.010);
        mRms.setRelease((Sample)0.100);

        reset();
    }

    // -----------------------------
    // Parameters
    // -----------------------------
    void setDetectorType(DetectorType t) { mDetType = t; }

    // Detector timing (seconds)
    void setAttackSeconds(Sample s) {
        mPeak.setAttack(s);
        mRms.setAttack(s);
    }
    void setReleaseSeconds(Sample s) {
        mPeak.setRelease(s);
        mRms.setRelease(s);
    }

    // Static curve
    void setThresholdDb(Sample db) { mCurve.setThreshold(db); }
    void setKneeDb(Sample db)      { mCurve.setKnee(std::max((Sample)0, db)); }
    void setMode(Mode m)           { mCurve.setMode(m); }

    // Ratio: for Downward/Upward compression.
    // (Gate mode uses ThresholdComparator's gate logic; ratio ignored.)
    void setRatio(Sample r) { mRatio = std::max((Sample)1, r); }

    // Makeup gain (output stage)
    void setMakeupDb(Sample db) { mMakeupLin = dbToLin<Sample>(db); }

    // Parallel blend (0=dry, 1=wet)
    void setDryWet(Sample wet) { mWet = std::clamp(wet, (Sample)0, (Sample)1); }

    // Sidechain: blend internal/external (0=internal, 1=external)
    void setSidechainBlend(Sample mix) { mScMix = std::clamp(mix, (Sample)0, (Sample)1); }

    // Optional sidechain filter (HPF/LPF/EQ/etc.)
    // Provide a functor/lambda: Sample f(Sample x)
    void setSidechainFilter(std::function<Sample(Sample)> f) { mScFilter = std::move(f); }
    void clearSidechainFilter() { mScFilter = {}; }

    // Lookahead delay in samples
    void setLookaheadSamples(int samples) {
        mLookahead.setDelay(samples);
        mLookaheadSamples = samples;
    }

    // -----------------------------
    // State / meters
    // -----------------------------
    void reset()
    {
        mLookahead.reset();
        mPeak.reset((Sample)0);
        mRms.reset((Sample)0);
        mEnv = (Sample)0;
        mGainLin = (Sample)1;
        mGainDb = (Sample)0;
        mGrDb = (Sample)0;
    }

    // Current detector envelope (linear)
    Sample env() const { return mEnv; }

    // Current applied gain (linear)
    Sample gainLin() const { return mGainLin; }

    // Current applied gain (dB; negative for attenuation)
    Sample gainDb() const { return mGainDb; }

    // Gain reduction in dB (positive value means reduction)
    Sample gainReductionDb() const { return mGrDb; }

    // -----------------------------
    // Processing (mono)
    // Provide extKey if you want external sidechain; otherwise pass 0.
    // -----------------------------
    Sample process(Sample x, Sample extKey = (Sample)0)
    {
        // 1) Build sidechain signal (internal/external blend)
        Sample sc = ((Sample)1 - mScMix) * x + mScMix * extKey;

        // 2) Optional sidechain filtering
        if (mScFilter) sc = mScFilter(sc);

        // 3) Level detection (envelope, linear)
        if (mDetType == DetectorType::Peak) {
            mEnv = mPeak.process(sc);
        } else {
            mEnv = mRms.process(sc);
        }

        // 4) Static curve (threshold + knee + mode) in dB-domain "delta"
        // ThresholdComparator::process returns:
        // - Downward: 0..+ (how far above threshold, with soft knee)
        // - Upward:  - (boost amount)
        // - Gate:    + (how far below threshold)
        const Sample cDb = mCurve.process(mEnv);

        // 5) Ratio stage (compressor modes)
        // For downward compression, gain reduction (positive) is:
        // GR = c * (1 - 1/R)
        // Then applied gain in dB is -GR
        Sample gainDb = (Sample)0;

        // NOTE: Gate mode from ThresholdComparator already returns "attenuation needed"
        // below threshold. We apply it as negative gain directly (no ratio).
        if (mCurrentMode() == Gate) {
            const Sample gateAttenDb = std::max((Sample)0, cDb);
            gainDb = -gateAttenDb;
        }
        else if (mCurrentMode() == Upward) {
            // Upward: cDb is negative (as coded). Apply ratio-like scaling if desired.
            // We'll map magnitude using (1 - 1/R) similarly, then keep sign.
            const Sample mag = std::abs(cDb);
            const Sample scaled = mag * ((Sample)1 - (Sample)1 / mRatio);
            gainDb = +scaled; // upward compression boosts => positive gain
        }
        else {
            // Downward
            const Sample grDb = std::max((Sample)0, cDb) * ((Sample)1 - (Sample)1 / mRatio);
            gainDb = -grDb;
        }

        // Metering
        mGainDb = gainDb;
        mGrDb   = std::max((Sample)0, -gainDb);

        // 6) Convert to linear gain
        const Sample gLin = dbToLin<Sample>(gainDb);
        mGainLin = gLin;

        // 7) Lookahead audio path
        const Sample xDelayed = (mLookaheadSamples > 0) ? mLookahead.process(x) : x;

        // 8) Apply gain + makeup
        const Sample wet = xDelayed * gLin * mMakeupLin;

        // 9) Parallel blend
        return ((Sample)1 - mWet) * x + mWet * wet;
    }

    void processBuffer(const Sample* in, Sample* out, std::size_t n, const Sample* extKey = nullptr)
    {
        if (!extKey) {
            for (std::size_t i = 0; i < n; ++i) out[i] = process(in[i], (Sample)0);
        } else {
            for (std::size_t i = 0; i < n; ++i) out[i] = process(in[i], extKey[i]);
        }
    }

private:
    // We can query the comparator mode only if we store it too.
    // Your ThresholdComparator stores it privately, so we mirror it here.
    // Keep these in sync by calling setMode().
    Mode mModeMirror = Downward;
    Mode mCurrentMode() const { return mModeMirror; }

public:
    // Override setMode to also mirror it (so we can branch without modifying ThresholdComparator)
    void setModeMirrored(Mode m) { mModeMirror = m; mCurve.setMode(m); }

private:
    DetectorType mDetType = DetectorType::ExpRMS;

    // Detection
    PeakDetector<Sample>     mPeak;
    ExpRMSFollower<Sample>  mRms;
    Sample                  mEnv = (Sample)0;

    // Curve + ratio
    ThresholdComparator<Sample> mCurve;
    Sample                     mRatio = (Sample)4;

    // Sidechain
    Sample mScMix = (Sample)0;
    std::function<Sample(Sample)> mScFilter;

    // Lookahead
    LookaheadBuffer<Sample> mLookahead;
    int mLookaheadSamples = 0;

    // Output stage
    Sample mMakeupLin = (Sample)1;
    Sample mWet = (Sample)1;

    // Meters
    Sample mGainLin = (Sample)1;
    Sample mGainDb  = (Sample)0;
    Sample mGrDb    = (Sample)0;
};
