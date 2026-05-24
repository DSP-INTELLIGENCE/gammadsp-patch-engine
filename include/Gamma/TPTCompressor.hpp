#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

#include "TPTEnvelopeFollower.hpp"
#include "AnalogVCA.hpp"

namespace gam {

template<class Tv>
static inline Tv dbToLin(Tv db){ return std::pow(Tv(10), db / Tv(20)); }

template<class Tv>
static inline Tv linToDb(Tv x){
    x = std::max(x, Tv(1e-20));
    return Tv(20) * std::log10(x);
}

/// ------------------------------------------------------------
/// TPTCompressor
/// - Detector: TPTEnvelopeFollower (Peak/RMS + A/R ms + sidechain HP)
/// - Gain computer: threshold/ratio/knee in dB
/// - VCA: AnalogVCA for character
/// - Gamma-style: operator(), reset(), onDomainChange()
/// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TPTCompressor : public Td {
public:
    using Detector = TPTEnvelopeFollower<Tv, Td>;
    using VCA      = AnalogVCA<Tv, Td>;

    enum class DetectorMode { Peak, RMS };

    TPTCompressor(){
        reset();
        onDomainChange(1.0);
    }

    // ---------------- params: dynamics ----------------
    void thresholdDb(Tv db){ mThrDb = db; }
    void ratio(Tv r){ mRatio = std::max(r, Tv(1)); }
    void kneeDb(Tv db){ mKneeDb = std::max(db, Tv(0)); }

    // ---------------- params: timing (detector A/R) ----------------
    void attackMs(Tv ms){ mDet.setAttackMs(ms); }
    void releaseMs(Tv ms){ mDet.setReleaseMs(ms); }

    void detector(DetectorMode m){
        mDetMode = m;
        mDet.setMode(m == DetectorMode::RMS ? Detector::RMS : Detector::PEAK);
    }

    // Sidechain HPF (detector path)
    void sidechainHPHz(Tv hz){ mDet.setSidechainHPHz(hz); }

    // ---------------- params: output ----------------
    void makeupDb(Tv db){ mMakeup = dbToLin(db); }
    void wet(Tv w){ mWet = std::clamp(w, Tv(0), Tv(1)); }

    // VCA character passthroughs
    void vcaModel(typename VCA::Model m){ mVCA.model(m); }
    void vcaSaturation(Tv s){ mVCA.saturation(s); }
    void vcaTHD(Tv d){ mVCA.thd(d); }
    void vcaBleed(Tv b){ mVCA.bleed(b); }
    void vcaAsymmetry(Tv a){ mVCA.asymmetry(a); }

    // ---------------- metering ----------------
    Tv envLin() const { return mEnvLin; }
    Tv envDb()  const { return mEnvDb; }
    Tv gainDb() const { return mGainDb; }    // <= 0 (negative = gain reduction)
    Tv grDb()   const { return -mGainDb; }   // >= 0

    // ---------------- lifecycle ----------------
    void reset(){
        mDet.reset();
        mVCA.reset();

        mEnvLin = Tv(0);
        mEnvDb  = Tv(-120);
        mGainDb = Tv(0);
    }

    void onDomainChange(double){
        mDet.onDomainChange(1.0);
        mVCA.onDomainChange(1.0);
    }

    // ---------------- processing ----------------
    inline Tv operator()(Tv x){
        return (*this)(x, x);
    }

    /// x = input, sc = sidechain signal
    inline Tv operator()(Tv x, Tv sc){
        // 1) Detect envelope (linear)
        mEnvLin = mDet(sc);
        mEnvDb  = mDet.valueDb(Tv(-120));

        // 2) Gain computer -> desired gain (dB, <= 0)
        mGainDb = computeGainDb(mEnvDb);

        // 3) Apply gain (linear) through VCA (with makeup)
        const Tv gLin = dbToLin(mGainDb) * mMakeup;
        Tv y = mVCA(x, gLin);

        // 4) Outer wet/dry (keep VCA wet = 1 for sanity, or use one wet control)
        return (Tv(1) - mWet) * x + mWet * y;
    }

private:
    // Soft-knee gain computer: returns gain in dB (<= 0)
    inline Tv computeGainDb(Tv inDb) const {
        if(mRatio <= Tv(1)) return Tv(0);

        const Tv thr  = mThrDb;
        const Tv knee = mKneeDb;

        // (1/R - 1) is <= 0, so gainDb will be <= 0 when over threshold
        const Tv invRatioMinus1 = (Tv(1) / mRatio) - Tv(1);

        if(knee <= Tv(0)){
            const Tv over = inDb - thr;
            if(over <= Tv(0)) return Tv(0);
            return invRatioMinus1 * over;
        }

        const Tv half = knee * Tv(0.5);

        if(inDb <= thr - half) return Tv(0);

        if(inDb >= thr + half){
            const Tv over = inDb - thr;
            return invRatioMinus1 * over;
        }

        // Inside knee region: quadratic soft knee
        const Tv u = inDb - (thr - half); // 0..knee
        return invRatioMinus1 * (u * u) / (knee * Tv(2));
    }

private:
    // user params
    Tv mThrDb  { Tv(-18) };
    Tv mRatio  { Tv(4) };
    Tv mKneeDb { Tv(6) };

    Tv mMakeup { Tv(1) };
    Tv mWet    { Tv(1) };

    DetectorMode mDetMode { DetectorMode::Peak };

    // components
    Detector mDet;
    VCA      mVCA;

    // meter/state
    Tv mEnvLin { Tv(0) };
    Tv mEnvDb  { Tv(-120) };
    Tv mGainDb { Tv(0) };
};

} // namespace gam
