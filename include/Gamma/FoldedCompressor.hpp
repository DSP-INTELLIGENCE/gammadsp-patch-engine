#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

// assumes these already exist:
#include "AttackGenerator.hpp"
#include "ReleaseGenerator.hpp"
#include "EvenHarmonicColor.hpp"
#include "FoldedVCA.hpp"


namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class FoldedCompressor : public Td {
public:
    // -------------------------------------------------
    // Subsystems
    // -------------------------------------------------
    AttackGenerator<Tv, Td>  attack;
    ReleaseGenerator<Tv, Td> release;

    EvenHarmonicColor<Tv, Td> even;
    FoldedVCA<Tv, Td> folded;
    BlockDC<Tv> dc;

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

    void foldAmount(Tv a){ mFoldAmt = scl::clip(a, Tv(0), Tv(1)); }
    void colorAmount(Tv a){ mColorAmt = scl::clip(a, Tv(0), Tv(1)); }

    void mix(Tv m){ mMix = scl::clip(m, Tv(0), Tv(1)); }

    // -------------------------------------------------
    // Lifecycle
    // -------------------------------------------------
    void reset()
    {
        mEnv = Tv(0);
        attack.reset();
        release.reset();
        even.reset();
        folded.reset();
        dc.reset();
    }

    // -------------------------------------------------
    // Processing
    // -------------------------------------------------
    inline Tv operator()(Tv x)
    {
        // --- detector (RMS-ish)
        const Tv ax = std::abs(x);
        mEnv += mEnvCoef * (ax - mEnv);

        // --- level in dB
        const Tv levelDb = linToDb(mEnv);

        // --- static curve
        Tv grDb = Tv(0);
        if(levelDb > mThreshDb){
            grDb = (levelDb - mThreshDb) * (Tv(1) - Tv(1) / mRatio);
        }

        // --- smooth gain reduction (attack / release)
        const Tv grSmoothDb =
            (grDb > mGrDb)
                ? attack(grDb)
                : release(grDb);

        mGrDb = grSmoothDb;

        // --- linear gain
        const Tv g = dbToLin(-grSmoothDb);

        // -------------------------------------------------
        // Wet path
        // -------------------------------------------------
        Tv y = x * g;

        // gain-reduction amount in [0..1]
        const Tv grNorm = scl::clip(grSmoothDb / Tv(24), Tv(0), Tv(1));

        // ---- even-harmonic color (GR driven)
        even.amount(mColorAmt * grNorm);
        y = even(y);

        // ---- folded VCA (GR + thermal driven)
        folded.foldAmount(mFoldAmt);
        y = folded(y, g);

        // ---- DC cleanup
        y = dc(y);

        // ---- makeup
        y *= mMakeup;

        // -------------------------------------------------
        // Dry / Wet
        // -------------------------------------------------
        return (Tv(1) - mMix) * x + mMix * y;
    }

    // -------------------------------------------------
    // Gamma domain hook
    // -------------------------------------------------
    void onDomainChange(double r)
    {
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // detector (~10 Hz RMS)
        mEnvCoef = Tv(1) - std::exp(-Tv(1) / (Tv(0.01) * fs));

        attack.onDomainChange(r);
        release.onDomainChange(r);
        even.onDomainChange(r);
        folded.onDomainChange(r);
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
    Tv mEnv      { Tv(0) };
    Tv mEnvCoef  { Tv(0) };

    Tv mGrDb     { Tv(0) };

    // -------------------------------------------------
    // Parameters
    // -------------------------------------------------
    Tv mThreshDb { Tv(-18) };
    Tv mRatio    { Tv(4) };

    Tv mMakeup   { Tv(1) };

    Tv mFoldAmt  { Tv(0.5) };
    Tv mColorAmt { Tv(0.5) };

    Tv mMix      { Tv(1) };
};

} // namespace gam
