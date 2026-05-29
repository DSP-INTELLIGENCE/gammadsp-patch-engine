// CharacterEngine.hpp
#pragma once
#include <algorithm>
#include <cmath>

// A detector "character engine" that morphs between:
// Clean → VCA → FET → Optical → Tube
//
// This produces a CONTROL ENVELOPE (linear), intended to feed your
// ThresholdComparator / gain computer.
//
// Design goals:
// - No allocations
// - Stable, smooth morphing (continuous crossfade between models)
// - Each "character" is mostly implemented by detector nonlinearity + envelope topology
//
// Assumes: gam::sampleRate() exists and returns Sample (or convertible).
// If your gam API is different, swap that call in sr().
namespace detchar {

template <class Sample>
static inline Sample sr() { return (Sample)gam::sampleRate(); }

template <class Sample>
static inline Sample clamp01(Sample x) { return std::clamp(x, (Sample)0, (Sample)1); }

template <class Sample>
static inline Sample dbToLin(Sample db) { return std::pow((Sample)10, db / (Sample)20); }

template <class Sample>
static inline Sample linToDb(Sample x, Sample floorLin = (Sample)1e-12)
{
    x = std::max(x, floorLin);
    return (Sample)20 * std::log10(x);
}

// One-pole smoother with selectable coefficient per-sample.
template <class Sample>
struct OnePole {
    Sample y = 0;
    void reset(Sample v = 0) { y = std::max((Sample)0, v); }
    Sample processWithCoeff(Sample x, Sample coeff) {
        y = ((Sample)1 - coeff) * x + coeff * y;
        return y;
    }
};

// Compute one-pole coefficient from time constant (seconds)
template <class Sample>
static inline Sample coeffFromTau(Sample tauSec)
{
    tauSec = std::max((Sample)1e-6, tauSec);
    Sample a = std::exp((Sample)-1 / (tauSec * sr<Sample>()));
    return std::clamp(a, (Sample)0, (Sample)0.9999999);
}

// Soft clip / saturation for detector shaping
template <class Sample>
static inline Sample softSat(Sample x, Sample drive)
{
    // drive >= 1 : more saturation
    drive = std::max((Sample)1, drive);
    return std::tanh(drive * x);
}

// Diode-ish asymmetry (FET-ish detector)
template <class Sample>
static inline Sample diodeRectify(Sample x, Sample asym)
{
    // asym in [0..1], 0 = symmetric, 1 = strong asymmetry (negative much smaller)
    asym = clamp01(asym);
    Sample negScale = (Sample)1 - (Sample)0.85 * asym; // ~0.15 at asym=1
    return (x >= (Sample)0) ? x : (negScale * x);
}

// Log compression inside detector (tube/broadcast-ish)
template <class Sample>
static inline Sample logWarp(Sample x, Sample k)
{
    // x assumed >= 0. k controls amount.
    k = std::max((Sample)0, k);
    return std::log1p(k * x) / std::log1p(k + (Sample)1); // normalized-ish
}

} // namespace detchar


template <class Sample>
class CharacterEngine {
public:
    // Morph axis: 0..1 maps across 5 characters
    // 0.00 Clean
    // 0.25 VCA
    // 0.50 FET
    // 0.75 Optical
    // 1.00 Tube
    void setMorph(Sample m) { mMorph = detchar::clamp01(m); }

    // Base time constants (seconds). Each character derives its own internal A/R from these.
    void setAttack(Sample s)  { mBaseAtk = std::max((Sample)1e-6, s); updateCoeffs(); }
    void setRelease(Sample s) { mBaseRel = std::max((Sample)1e-6, s); updateCoeffs(); }

    // Detector shaping controls (optional)
    void setDrive(Sample d)      { mDrive = std::max((Sample)1, d); }
    void setFetAsym(Sample a01)  { mFetAsym = detchar::clamp01(a01); }
    void setTubeLogK(Sample k)   { mTubeLogK = std::max((Sample)0, k); }

    // Reset all states
    void reset(Sample initialEnv = (Sample)0)
    {
        initialEnv = std::max((Sample)0, initialEnv);
        mClean.reset(initialEnv);

        mVcaEnergy.reset(initialEnv * initialEnv);
        mVcaRms.reset(initialEnv);

        mFet.reset(initialEnv);

        mOptoFastEnergy.reset(initialEnv * initialEnv);
        mOptoSlowEnergy.reset(initialEnv * initialEnv);
        mOptoRms.reset(initialEnv);

        mTubeEnergy.reset(initialEnv * initialEnv);
        mTubeRms.reset(initialEnv);

        mEnv = initialEnv;
    }

    // Process one sample, return detector envelope (linear)
    Sample process(Sample x)
    {
        // Build weights for 5-way morph (triangular blending over 0..4)
        Sample t = mMorph * (Sample)4;
        Sample w[5];
        for (int i = 0; i < 5; ++i) {
            Sample d = std::abs(t - (Sample)i);
            w[i] = std::max((Sample)0, (Sample)1 - d);
        }
        Sample wsum = w[0]+w[1]+w[2]+w[3]+w[4];
        if (wsum > (Sample)0) for (auto &wi : w) wi /= wsum;

        // --- Character 0: Clean (neutral peak-ish detector)
        // Full-wave rectifier + clean A/R envelope
        Sample envClean = detectPeakAbs(x, mCleanAtk, mCleanRel, mClean);

        // --- Character 1: VCA (RMS-ish energy detector, fairly "gluey")
        // Energy-domain A/R on x^2 (your ExpRMSFollower style)
        Sample envVca = detectEnergyRms(x, mVcaAtk, mVcaRel, /*warp*/Warp::None);

        // --- Character 2: FET (aggressive peak, diode-ish rectifier + program-ish release)
        Sample envFet = detectFet(x);

        // --- Character 3: Optical (dual-stage: fast+slow energy blend, slow release feel)
        Sample envOpto = detectOpto(x);

        // --- Character 4: Tube (softer, compressed detector: saturation + log warp + RMS-ish)
        Sample envTube = detectTube(x);

        // Blend
        mEnv = w[0]*envClean + w[1]*envVca + w[2]*envFet + w[3]*envOpto + w[4]*envTube;
        mEnv = std::max(mEnv, kFloor);
        return mEnv;
    }

    Sample value() const { return mEnv; }
    Sample valueDb(Sample floorDb = (Sample)-120) const
    {
        Sample db = detchar::linToDb(mEnv, kFloor);
        return std::max(db, floorDb);
    }

private:
    static constexpr Sample kFloor = (Sample)1e-12;

    enum class Warp { None, Sat, SatLog };

    void updateCoeffs()
    {
        // Clean: fastest/most literal peak tracking
        mCleanAtk = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)0.80);
        mCleanRel = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)0.90);

        // VCA: a touch slower, glue
        mVcaAtk   = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)1.40);
        mVcaRel   = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)1.20);

        // FET: very fast attack, program-ish release (fast+slow)
        mFetAtk   = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)0.35);
        mFetRelFast = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)0.35);
        mFetRelSlow = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)2.50);

        // Optical: fast catches transients, slow dominates body/release
        mOptoAtkFast = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)0.80);
        mOptoRelFast = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)0.90);
        mOptoAtkSlow = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)2.50);
        mOptoRelSlow = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)6.00);

        // Tube: slower + detector compression
        mTubeAtk  = detchar::coeffFromTau<Sample>(mBaseAtk * (Sample)1.80);
        mTubeRel  = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)2.20);
    }

    // Peak abs detector with split A/R coefficients (coeffs are the "a" in y=(1-a)x + a y)
    static Sample detectPeakAbs(Sample x, Sample atkCoeff, Sample relCoeff, detchar::OnePole<Sample>& st)
    {
        Sample a = std::abs(x);
        Sample coeff = (a > st.y) ? atkCoeff : relCoeff;
        Sample y = st.processWithCoeff(a, coeff);
        if (y < kFloor) { st.y = 0; y = 0; }
        return y;
    }

    // Energy RMS detector with optional warping in the detector input (before squaring)
    Sample detectEnergyRms(Sample x, Sample atkCoeff, Sample relCoeff, Warp warp)
    {
        Sample a = x;

        // Optional detector nonlinearity
        if (warp == Warp::Sat || warp == Warp::SatLog) {
            a = detchar::softSat(a, mDrive);
        }

        Sample mag = std::abs(a);
        if (warp == Warp::SatLog) {
            mag = detchar::logWarp(mag, mTubeLogK);
        }

        Sample e = mag * mag;

        // Choose coeff based on energy rising/falling
        Sample coeff = (e > mVcaEnergy.y) ? atkCoeff : relCoeff;
        Sample energy = mVcaEnergy.processWithCoeff(e, coeff);
        energy = std::max(energy, kFloor);

        Sample rms = std::sqrt(energy);
        mVcaRms.y = rms;
        return rms;
    }

    // FET-style detector: diode asymmetry + very fast attack + program-ish two-stage release
    Sample detectFet(Sample x)
    {
        // Asymmetric "diode" rectification
        Sample xd = detchar::diodeRectify(x, mFetAsym);
        Sample a  = std::abs(xd);

        // Attack: very fast
        Sample atk = mFetAtk;
        Sample yAtk = mFet.processWithCoeff(a, (a > mFet.y) ? atk : atk); // same coeff path; we handle release below

        // Program-ish release: blend fast/slow releases depending on level
        // Higher envelope -> more slow component (hang), lower -> faster recovery.
        Sample level = detchar::clamp01(yAtk * (Sample)2); // assumes typical audio in 0..1
        Sample relCoeff = level * mFetRelSlow + ((Sample)1 - level) * mFetRelFast;

        // If falling, apply release; if rising, keep attack
        if (a <= mFet.y) {
            mFet.y = ((Sample)1 - relCoeff) * a + relCoeff * mFet.y;
        } else {
            mFet.y = ((Sample)1 - atk) * a + atk * mFet.y;
        }

        if (mFet.y < kFloor) mFet.y = 0;
        return mFet.y;
    }

    // Optical: dual energy envelopes (fast+slow) blended, then sqrt
    Sample detectOpto(Sample x)
    {
        Sample a = std::abs(x);
        Sample e = a * a;

        // Fast energy track
        {
            Sample coeff = (e > mOptoFastEnergy.y) ? mOptoAtkFast : mOptoRelFast;
            mOptoFastEnergy.processWithCoeff(e, coeff);
        }
        // Slow energy track
        {
            Sample coeff = (e > mOptoSlowEnergy.y) ? mOptoAtkSlow : mOptoRelSlow;
            mOptoSlowEnergy.processWithCoeff(e, coeff);
        }

        // Blend: optical tends to be dominated by slow component with a bit of fast transient capture
        Sample blend = (Sample)0.25; // fast amount
        Sample energy = blend * mOptoFastEnergy.y + ((Sample)1 - blend) * mOptoSlowEnergy.y;

        energy = std::max(energy, kFloor);
        Sample rms = std::sqrt(energy);

        // Mild extra smoothing on amplitude for the “opto” feel
        mOptoRms.processWithCoeff(rms, detchar::coeffFromTau<Sample>(mBaseRel * (Sample)0.25));
        return std::max(mOptoRms.y, (Sample)0);
    }

    // Tube: saturated + log-warped detector feeding energy RMS
    Sample detectTube(Sample x)
    {
        // Saturate first to reduce peak dominance, then log-warp to compress detector dynamic range
        Sample a = detchar::softSat(x, mDrive);
        Sample mag = detchar::logWarp(std::abs(a), mTubeLogK);
        Sample e = mag * mag;

        Sample coeff = (e > mTubeEnergy.y) ? mTubeAtk : mTubeRel;
        Sample energy = mTubeEnergy.processWithCoeff(e, coeff);
        energy = std::max(energy, kFloor);

        Sample rms = std::sqrt(energy);

        // Optional extra “tube sag”: very gentle smoothing on output RMS
        Sample sagCoeff = detchar::coeffFromTau<Sample>(mBaseRel * (Sample)0.15);
        mTubeRms.processWithCoeff(rms, sagCoeff);

        return std::max(mTubeRms.y, (Sample)0);
    }

private:
    // User params
    Sample mMorph    = (Sample)0;      // 0..1
    Sample mBaseAtk  = (Sample)0.010;  // seconds
    Sample mBaseRel  = (Sample)0.100;  // seconds
    Sample mDrive    = (Sample)1.5;    // saturation drive
    Sample mFetAsym  = (Sample)0.6;    // diode asymmetry
    Sample mTubeLogK = (Sample)12.0;   // log warp strength

    // Output
    Sample mEnv = (Sample)0;

    // States per character
    detchar::OnePole<Sample> mClean;

    detchar::OnePole<Sample> mVcaEnergy;
    detchar::OnePole<Sample> mVcaRms;

    detchar::OnePole<Sample> mFet;

    detchar::OnePole<Sample> mOptoFastEnergy;
    detchar::OnePole<Sample> mOptoSlowEnergy;
    detchar::OnePole<Sample> mOptoRms;

    detchar::OnePole<Sample> mTubeEnergy;
    detchar::OnePole<Sample> mTubeRms;

    // Coefficients derived from base times
    Sample mCleanAtk = 0, mCleanRel = 0;

    Sample mVcaAtk = 0, mVcaRel = 0;

    Sample mFetAtk = 0, mFetRelFast = 0, mFetRelSlow = 0;

    Sample mOptoAtkFast = 0, mOptoRelFast = 0, mOptoAtkSlow = 0, mOptoRelSlow = 0;

    Sample mTubeAtk = 0, mTubeRel = 0;

public:
    // Ensure coeffs initialized even if user doesn't set times
    CharacterEngine() { updateCoeffs(); reset(); }
};
