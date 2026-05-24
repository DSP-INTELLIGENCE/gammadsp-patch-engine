#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "AttackGenerator.hpp"
#include "ReleaseGenerator.hpp"
#include "EvenHarmonicsGenerator.hpp"
#include "TPT1Pole.hpp"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AnalogBusCompressor : public Td {
public:
    // -------------------------------------------------
    // Subsystems
    // -------------------------------------------------
    AttackGenerator<Tv, Td>  attack;
    ReleaseGenerator<Tv, Td> release;

    EvenHarmonicEnhancer<Tv, Td> color;
    DCBlocker1P<Tv, Td>          dc;

    // -------------------------------------------------
    // User parameters
    // -------------------------------------------------
    void thresholdDb(Tv db){ mThreshDb = db; }
    void ratio(Tv r){ mRatio = std::max(Tv(1), r); }

    void attackTime(Tv s){ attack.time(s); }
    void releaseTime(Tv s){ release.time(s); }

    void makeupDb(Tv db){
        mMakeup = std::pow(Tv(10), db / Tv(20));
    }

    /// overall color intensity [0..1]
    void colorAmount(Tv a){
        mColorAmt = scl::clip(a, Tv(0), Tv(1));
    }

    void mix(Tv m){
        mMix = scl::clip(m, Tv(0), Tv(1));
    }

    // -------------------------------------------------
    // Lifecycle
    // -------------------------------------------------
    void reset()
    {
        mEnv = Tv(0);
        mGrDb = Tv(0);

        attack.reset();
        release.reset();
        color.reset();
        dc.reset();
    }

    // -------------------------------------------------
    // Processing
    // -------------------------------------------------
    inline Tv operator()(Tv x)
    {
        // -----------------------------
        // Detector (RMS-ish envelope)
        // -----------------------------
        const Tv ax = std::abs(x);
        mEnv += mEnvCoef * (ax - mEnv);

        // -----------------------------
        // Level → dB
        // -----------------------------
        const Tv levelDb = linToDb(mEnv);

        // -----------------------------
        // Static compression curve
        // -----------------------------
        Tv grDb = Tv(0);
        if(levelDb > mThreshDb){
            grDb = (levelDb - mThreshDb)
                 * (Tv(1) - Tv(1) / mRatio);
        }

        // -----------------------------
        // Attack / Release smoothing
        // -----------------------------
        const Tv grSmoothDb =
            (grDb > mGrDb)
                ? attack(grDb)
                : release(grDb);

        mGrDb = grSmoothDb;

        // -----------------------------
        // Linear gain
        // -----------------------------
        const Tv g = dbToLin(-grSmoothDb);

        // -----------------------------
        // Wet path
        // -----------------------------
        Tv y = x * g;

        // -----------------------------
        // GR-driven even-harmonic color
        // -----------------------------
        // Normalize GR: 0..~12 dB → 0..1
        const Tv grNorm =
            scl::clip(grSmoothDb / Tv(12), Tv(0), Tv(1));

        color.amount(mColorAmt * grNorm);
        y = color(y);

        // -----------------------------
        // DC cleanup
        // -----------------------------
        y = dc(y);

        // -----------------------------
        // Makeup
        // -----------------------------
        y *= mMakeup;

        // -----------------------------
        // Dry / Wet
        // -----------------------------
        return (Tv(1) - mMix) * x + mMix * y;
    }

    // -------------------------------------------------
    // Gamma domain hook
    // -------------------------------------------------
    void onDomainChange(double r)
    {
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // RMS detector (~20 ms, classic bus behavior)
        mEnvCoef = Tv(1) - std::exp(-Tv(1) / (Tv(0.02) * fs));

        attack.onDomainChange(r);
        release.onDomainChange(r);
        color.onDomainChange(r);
        dc.onDomainChange(r);
    }

private:
    // -------------------------------------------------
    // Utilities
    // -------------------------------------------------
    static inline Tv linToDb(Tv x){
        return Tv(20) * std::log10(std::max(x, Tv(1e-12)));
    }

    static inline Tv dbToLin(Tv db){
        return std::pow(Tv(10), db / Tv(20));
    }

private:
    // -------------------------------------------------
    // State
    // -------------------------------------------------
    Tv mEnv     { Tv(0) };
    Tv mEnvCoef { Tv(0) };

    Tv mGrDb    { Tv(0) };

    // -------------------------------------------------
    // Parameters
    // -------------------------------------------------
    Tv mThreshDb { Tv(-18) };
    Tv mRatio    { Tv(2) };

    Tv mMakeup   { Tv(1) };
    Tv mColorAmt { Tv(0.3) };
    Tv mMix      { Tv(1) };
};

} // namespace gam
