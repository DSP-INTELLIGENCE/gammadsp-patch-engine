// DynamicsProcessorStereo.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

// Your existing building blocks:
#include "LookaheadBuffer.hpp"
#include "CharacterEngine.hpp"
#include "ThresholdComparator.hpp"
#include "GainController.hpp"
#include "AdaptiveTimeConstants.hpp"
#include "PeakDetector.hpp"
#include "ExpRMSFollower.hpp"

// ============================================================
// Utilities
// ============================================================
template <class Sample>
static inline Sample dbToLin(Sample db) { return std::pow((Sample)10, db / (Sample)20); }

template <class Sample>
static inline Sample linToDb(Sample x, Sample floorLin = (Sample)1e-12)
{
    x = std::max(x, floorLin);
    return (Sample)20 * std::log10(x);
}

template <class Sample>
static inline Sample softClipTanh(Sample x, Sample drive = (Sample)1)
{
    drive = std::max((Sample)1, drive);
    return std::tanh(drive * x) / std::tanh(drive);
}

template <class Sample>
static inline Sample hardClip(Sample x, Sample ceiling = (Sample)1)
{
    return std::clamp(x, -ceiling, ceiling);
}

// ============================================================
// Gain Control Stage (VCA-style digital multiplier)
// - applies smoothed gain (linear) + makeup + dry/wet
// ============================================================
template <class Sample>
class GainStage {
public:
    void setMakeupDb(Sample db) { mMakeup = dbToLin<Sample>(db); }
    void setDryWet(Sample wet)  { mWet = std::clamp(wet, (Sample)0, (Sample)1); }

    void reset() { /* stateless */ }

    inline Sample process(Sample dry, Sample gainLin) const
    {
        const Sample wet = dry * gainLin * mMakeup;
        return ((Sample)1 - mWet) * dry + mWet * wet;
    }

private:
    Sample mMakeup = (Sample)1;
    Sample mWet    = (Sample)1;
};

// ============================================================
// Control Signal Generator (gain smoothing in dB domain)
// - env (linear) -> curve -> target gain dB -> adaptive timing -> GainController -> gainLin
// ============================================================
template <class Sample>
class ControlSignalGenerator {
public:
    void setThresholdDb(Sample tDb) { mCurve.setThreshold(tDb); }
    void setKneeDb(Sample kDb)      { mCurve.setKnee(std::max((Sample)0, kDb)); }
    void setRatio(Sample r)         { mRatio = std::max((Sample)1, r); }
    void setMode(Mode m)            { mModeMirror = m; mCurve.setMode(m); }

    void setAttack(Sample sec)      { mBaseAtk = std::max((Sample)1e-6, sec); }
    void setRelease(Sample sec)     { mBaseRel = std::max((Sample)1e-6, sec); }
    void setHold(Sample sec)        { mGainCtrl.setHold(std::max((Sample)0, sec)); }

    // Adaptive timing
    void setAdaptiveAmount(Sample a) { mAdapt.setAdaptAmount(std::clamp(a, (Sample)0, (Sample)1)); }
    void setAdaptiveMemory(Sample m) { mAdapt.setMemory(std::clamp(m, (Sample)0, (Sample)1)); }

    void reset()
    {
        mGainCtrl.reset((Sample)0);
        mAdapt.reset();
        mGainDb = (Sample)0;
        mTargetDb = (Sample)0;
        mGainLin = (Sample)1;
        mGrDb = (Sample)0;
    }

    // env: detector envelope (linear)
    inline Sample process(Sample env)
    {
        env = std::max(env, kFloor);

        // Static curve: your ThresholdComparator returns c in dB “amount into curve” (sign depends on mode)
        const Sample cDb = mCurve.process(env);

        Sample targetGainDb = (Sample)0;

        if (mModeMirror == Gate)
        {
            // cDb is positive attenuation needed below threshold
            targetGainDb = -std::max((Sample)0, cDb);
        }
        else if (mModeMirror == Upward)
        {
            // cDb is negative (your comparator returns -c). Convert to positive boost.
            const Sample boost = std::abs(cDb) * ((Sample)1 - (Sample)1 / mRatio);
            targetGainDb = +boost;
        }
        else
        {
            // Downward: cDb is positive “above threshold”; apply ratio to get GR
            const Sample gr = std::max((Sample)0, cDb) * ((Sample)1 - (Sample)1 / mRatio);
            targetGainDb = -gr;
        }

        // Adaptive time constants compute current attack/release based on target vs current
        Sample atk, rel;
        mAdapt.compute(targetGainDb, mGainCtrl.valueDb(), atk, rel);

        // If user never set base times, seed adapt with base
        // (We keep adapt’s base values in sync each sample to keep this class self-contained.)
        mAdapt.setBaseAttack(mBaseAtk);
        mAdapt.setBaseRelease(mBaseRel);

        mGainCtrl.setAttack(atk);
        mGainCtrl.setRelease(rel);

        const Sample smoothGainDb = mGainCtrl.process(targetGainDb);

        mTargetDb = targetGainDb;
        mGainDb   = smoothGainDb;
        mGrDb     = std::max((Sample)0, -smoothGainDb);
        mGainLin  = dbToLin<Sample>(smoothGainDb);
        return mGainLin;
    }

    // meters
    Sample gainDb() const { return mGainDb; }              // can be + (upward) or - (downward/gate)
    Sample targetGainDb() const { return mTargetDb; }
    Sample gainReductionDb() const { return mGrDb; }       // positive amount
    Sample gainLin() const { return mGainLin; }

private:
    static constexpr Sample kFloor = (Sample)1e-12;

    ThresholdComparator<Sample> mCurve;
    GainController<Sample>      mGainCtrl;
    AdaptiveTimeConstants<Sample> mAdapt;

    Mode   mModeMirror = Downward;
    Sample mRatio      = (Sample)4;
    Sample mBaseAtk    = (Sample)0.01;
    Sample mBaseRel    = (Sample)0.10;

    Sample mGainDb   = (Sample)0;
    Sample mTargetDb = (Sample)0;
    Sample mGainLin  = (Sample)1;
    Sample mGrDb     = (Sample)0;
};

// ============================================================
// Limiter Safety Stage (post-compressor “catcher”)
// - clamps overs with a fast gain envelope
// - optional soft clipper for last-resort safety
// NOTE: This is not a full true-peak oversampled limiter; it’s a robust safety stage.
// ============================================================
template <class Sample>
class SafetyLimiter {
public:
    void setEnabled(bool b) { mEnabled = b; }
    void setCeilingDb(Sample db) { mCeiling = dbToLin<Sample>(db); } // e.g. -1.0 dBFS
    void setAttack(Sample sec) { mAtk.setAttack(sec); }              // e.g. 0.0005
    void setRelease(Sample sec){ mAtk.setRelease(sec); }             // e.g. 0.050
    void setSoftClip(bool b, Sample drive = (Sample)2) { mSoftClip = b; mDrive = drive; }

    void reset()
    {
        mAtk.reset((Sample)0);
        mGain.reset((Sample)1);
    }

    // Process stereo sample in-place (applies SAME safety gain to both channels)
    inline void process(Sample& l, Sample& r)
    {
        if (!mEnabled) return;

        // Detect post level (sample peak)
        const Sample p = std::max(std::abs(l), std::abs(r));
        const Sample peak = mAtk.process(p);

        // Needed gain to keep below ceiling
        Sample g = (peak > mCeiling) ? (mCeiling / std::max(peak, (Sample)1e-12)) : (Sample)1;

        // Smooth safety gain (fast down, slower up) using one-pole on linear gain
        const Sample coeff = (g < mGain.value()) ? mDownCoeff : mUpCoeff;
        mGain.set(((Sample)1 - coeff) * g + coeff * mGain.value());

        const Sample gs = mGain.value();
        l *= gs; r *= gs;

        // Last line of defense
        if (mSoftClip) {
            l = softClipTanh(l, mDrive);
            r = softClipTanh(r, mDrive);
        } else {
            l = hardClip(l, (Sample)1);
            r = hardClip(r, (Sample)1);
        }
    }

    Sample safetyGain() const { return mGain.value(); }

    // Call after setAttack/release if desired
    void updateSmoothers()
    {
        // You can tune these; they’re intentionally simple and stable
        mDownCoeff = std::exp((Sample)-1 / ((Sample)0.0005 * gam::sampleRate())); // ~0.5 ms
        mUpCoeff   = std::exp((Sample)-1 / ((Sample)0.0500 * gam::sampleRate())); // ~50 ms
    }

private:
    // tiny helper for smoothed linear gain
    struct SmoothLin {
        Sample v = (Sample)1;
        void reset(Sample x) { v = x; }
        Sample value() const { return v; }
        void set(Sample x) { v = x; }
    };

    bool   mEnabled = true;
    bool   mSoftClip = true;
    Sample mDrive = (Sample)2;

    Sample mCeiling = (Sample)0.8912509; // -1 dBFS default
    PeakDetector<Sample> mAtk;           // just used as a peak tracker for p

    SmoothLin mGain;

    // Smoother coefficients for safety gain itself
    Sample mDownCoeff = (Sample)0.0;
    Sample mUpCoeff   = (Sample)0.0;
};

// ============================================================
// Metering (simple, stable, useful)
// - input/output peak (dBFS)
// - input/output RMS (dBFS)
// - GR (dB)
// ============================================================
template <class Sample>
class StereoMeters {
public:
    void reset()
    {
        inPeak.reset(); outPeak.reset();
        inRms.reset((Sample)0); outRms.reset((Sample)0);
        grSmooth.reset((Sample)0);
        mInPeakDb = mOutPeakDb = (Sample)-120;
        mInRmsDb  = mOutRmsDb  = (Sample)-120;
        mGrDb     = (Sample)0;
    }

    // Call per-sample (or per-block with small downsample)
    inline void update(Sample inL, Sample inR, Sample outL, Sample outR, Sample grDb)
    {
        const Sample inP  = std::max(std::abs(inL),  std::abs(inR));
        const Sample outP = std::max(std::abs(outL), std::abs(outR));

        inPeak.process(inP);
        outPeak.process(outP);

        // RMS followers for metering (slow-ish)
        inRms.process((inL + inR) * (Sample)0.5);
        outRms.process((outL + outR) * (Sample)0.5);

        // Smooth GR meter
        grSmooth.process(std::max((Sample)0, grDb));

        mInPeakDb  = linToDb<Sample>(inPeak.value(),  (Sample)1e-12);
        mOutPeakDb = linToDb<Sample>(outPeak.value(), (Sample)1e-12);
        mInRmsDb   = (Sample)10 * std::log10(std::max(inRms.energy(),  (Sample)1e-12));
        mOutRmsDb  = (Sample)10 * std::log10(std::max(outRms.energy(), (Sample)1e-12));
        mGrDb      = grSmooth.value();
    }

    Sample inPeakDb() const { return mInPeakDb; }
    Sample outPeakDb() const { return mOutPeakDb; }
    Sample inRmsDb() const { return mInRmsDb; }
    Sample outRmsDb() const { return mOutRmsDb; }
    Sample grDb() const { return mGrDb; }

private:
    PeakDetector<Sample>      inPeak{ (Sample)0.001, (Sample)0.050 };
    PeakDetector<Sample>      outPeak{ (Sample)0.001, (Sample)0.050 };
    ExpRMSFollower<Sample>    inRms{ (Sample)0.300, (Sample)0.300 };
    ExpRMSFollower<Sample>    outRms{ (Sample)0.300, (Sample)0.300 };
    PeakDetector<Sample>      grSmooth{ (Sample)0.050, (Sample)0.200 };

    Sample mInPeakDb  = (Sample)-120;
    Sample mOutPeakDb = (Sample)-120;
    Sample mInRmsDb   = (Sample)-120;
    Sample mOutRmsDb  = (Sample)-120;
    Sample mGrDb      = (Sample)0;
};

// ============================================================
// Stereo Linking Modes
// ============================================================
enum class StereoLinkMode {
    Unlinked,   // independent L/R control signals
    Max,        // link by max(envL, envR)
    Average,    // link by 0.5*(envL+envR)
    RmsLink     // link by sqrt(0.5*(envL^2 + envR^2))
};

// ============================================================
// Full End-to-End Stereo Dynamics Processor
// - Sidechain analysis via CharacterEngine (per channel)
// - Stereo linking (env link and/or gain link by using one CSG)
// - Lookahead (audio path delay)
// - GainControlStage (VCA multiplier + makeup + dry/wet)
// - Safety limiter (post stage)
// - Meters
// ============================================================
template <class Sample>
class DynamicsProcessorStereo {
public:
    explicit DynamicsProcessorStereo(int maxLookaheadSamples = 4096)
    : mLookL(maxLookaheadSamples)
    , mLookR(maxLookaheadSamples)
    {
        // Defaults
        setMode(Downward);
        setThresholdDb((Sample)-24);
        setKneeDb((Sample)6);
        setRatio((Sample)4);

        setAttack((Sample)0.01);
        setRelease((Sample)0.10);
        setHold((Sample)0.0);

        setAdaptive((Sample)0.5, (Sample)0.9);

        setCharacterMorph((Sample)0.25); // ~VCA-ish
        setLookaheadSamples(0);

        setMakeupDb((Sample)0);
        setDryWet((Sample)1);

        setStereoLinkMode(StereoLinkMode::Max);

        // Safety limiter defaults
        mSafety.setEnabled(true);
        mSafety.setCeilingDb((Sample)-1.0);
        mSafety.setAttack((Sample)0.0005);
        mSafety.setRelease((Sample)0.050);
        mSafety.setSoftClip(true, (Sample)2);
        mSafety.updateSmoothers();

        reset();
    }

    // ---------- Controls ----------
    void setMode(Mode m) { mCsgL.setMode(m); mCsgR.setMode(m); mMode = m; }

    void setThresholdDb(Sample db) { mCsgL.setThresholdDb(db); mCsgR.setThresholdDb(db); }
    void setKneeDb(Sample db)      { mCsgL.setKneeDb(db);      mCsgR.setKneeDb(db); }
    void setRatio(Sample r)        { mCsgL.setRatio(r);        mCsgR.setRatio(r); }

    // Gain smoothing base times (seconds)
    void setAttack(Sample sec)  { mCsgL.setAttack(sec);  mCsgR.setAttack(sec); }
    void setRelease(Sample sec) { mCsgL.setRelease(sec); mCsgR.setRelease(sec); }
    void setHold(Sample sec)    { mCsgL.setHold(sec);    mCsgR.setHold(sec); }

    void setAdaptive(Sample amount, Sample memory)
    {
        mCsgL.setAdaptiveAmount(amount); mCsgR.setAdaptiveAmount(amount);
        mCsgL.setAdaptiveMemory(memory); mCsgR.setAdaptiveMemory(memory);
    }

    // Character detector (per channel)
    void setCharacterMorph(Sample morph01)
    {
        mCharL.setMorph(std::clamp(morph01, (Sample)0, (Sample)1));
        mCharR.setMorph(std::clamp(morph01, (Sample)0, (Sample)1));
    }
    void setCharacterDrive(Sample d) { mCharL.setDrive(d); mCharR.setDrive(d); }
    void setCharacterFetAsym(Sample a01) { mCharL.setFetAsym(a01); mCharR.setFetAsym(a01); }
    void setCharacterTubeLogK(Sample k) { mCharL.setTubeLogK(k); mCharR.setTubeLogK(k); }

    // Lookahead (audio path delay in samples)
    void setLookaheadSamples(int samples) { mLookL.setDelay(samples); mLookR.setDelay(samples); mLookahead = samples; }

    // Gain stage
    void setMakeupDb(Sample db) { mGain.setMakeupDb(db); }
    void setDryWet(Sample wet)  { mGain.setDryWet(wet); }

    // Stereo linking
    void setStereoLinkMode(StereoLinkMode m) { mLinkMode = m; }

    // Safety limiter
    void enableSafetyLimiter(bool b) { mSafety.setEnabled(b); }
    void setLimiterCeilingDb(Sample db) { mSafety.setCeilingDb(db); }
    void setLimiterSoftClip(bool b, Sample drive = (Sample)2) { mSafety.setSoftClip(b, drive); }

    // ---------- State ----------
    void reset()
    {
        mLookL.reset(); mLookR.reset();
        mCharL.reset(); mCharR.reset();
        mCsgL.reset();  mCsgR.reset();
        mSafety.reset();
        mMeters.reset();
        mLastGainL = mLastGainR = (Sample)1;
    }

    // ---------- Meters ----------
    const StereoMeters<Sample>& meters() const { return mMeters; }

    // ---------- Processing ----------
    inline void processSample(Sample inL, Sample inR, Sample& outL, Sample& outR,
                              Sample extKeyL = (Sample)0, Sample extKeyR = (Sample)0,
                              Sample scMix = (Sample)0 /* 0=internal, 1=external */)
    {
        // Sidechain source (simple blend internal/external per channel)
        Sample scL = ((Sample)1 - scMix) * inL + scMix * extKeyL;
        Sample scR = ((Sample)1 - scMix) * inR + scMix * extKeyR;

        // Character envelope per channel (linear)
        const Sample envL = mCharL.process(scL);
        const Sample envR = mCharR.process(scR);

        // Link envelope if requested
        Sample envLinkL = envL, envLinkR = envR;
        if (mLinkMode != StereoLinkMode::Unlinked)
        {
            Sample linked = envL;
            if (mLinkMode == StereoLinkMode::Max) {
                linked = std::max(envL, envR);
            } else if (mLinkMode == StereoLinkMode::Average) {
                linked = (envL + envR) * (Sample)0.5;
            } else { // RmsLink
                linked = std::sqrt((envL*envL + envR*envR) * (Sample)0.5);
            }
            envLinkL = envLinkR = linked;
        }

        // Generate gain control(s)
        Sample gainL = mCsgL.process(envLinkL);
        Sample gainR = (mLinkMode == StereoLinkMode::Unlinked) ? mCsgR.process(envLinkR) : gainL;

        mLastGainL = gainL; mLastGainR = gainR;

        // Lookahead delay on audio path
        const Sample xL = (mLookahead > 0) ? mLookL.process(inL) : inL;
        const Sample xR = (mLookahead > 0) ? mLookR.process(inR) : inR;

        // Apply gain stage (makeup + dry/wet)
        Sample yL = mGain.process(xL, gainL);
        Sample yR = mGain.process(xR, gainR);

        // Safety limiter (linked)
        mSafety.process(yL, yR);

        outL = yL;
        outR = yR;

        // Metering
        const Sample grDb = (mLinkMode == StereoLinkMode::Unlinked)
            ? (mCsgL.gainReductionDb() + mCsgR.gainReductionDb()) * (Sample)0.5
            : mCsgL.gainReductionDb();

        mMeters.update(inL, inR, outL, outR, grDb);
    }

    void processBuffer(const Sample* inL, const Sample* inR,
                       Sample* outL, Sample* outR,
                       std::size_t n,
                       const Sample* extKeyL = nullptr,
                       const Sample* extKeyR = nullptr,
                       Sample scMix = (Sample)0)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            Sample eL = extKeyL ? extKeyL[i] : (Sample)0;
            Sample eR = extKeyR ? extKeyR[i] : (Sample)0;
            processSample(inL[i], inR[i], outL[i], outR[i], eL, eR, scMix);
        }
    }

    // Useful live-readouts
    Sample gainLinL() const { return mLastGainL; }
    Sample gainLinR() const { return mLastGainR; }

private:
    // Audio path
    LookaheadBuffer<Sample> mLookL, mLookR;
    int mLookahead = 0;

    // Detector path
    CharacterEngine<Sample> mCharL, mCharR;

    // Control signal generators
    ControlSignalGenerator<Sample> mCsgL, mCsgR;
    Mode mMode = Downward;

    // Gain stage
    GainStage<Sample> mGain;

    // Safety limiter
    SafetyLimiter<Sample> mSafety;

    // Stereo link
    StereoLinkMode mLinkMode = StereoLinkMode::Max;

    // Meters
    StereoMeters<Sample> mMeters;

    // Cached last gains
    Sample mLastGainL = (Sample)1;
    Sample mLastGainR = (Sample)1;
};
