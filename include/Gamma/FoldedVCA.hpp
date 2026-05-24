// AnalogVCA_Fold.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

// include your wavefolder header
#include "BandlimitedWavefolder.hpp"


namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN, int OS = 4>
class FoldedVCA : public Td {
public:
    enum class Model { Clean, THAT2181, DBX, SSL };

    // ---- parameters (existing) ----
    void model(Model m)      { mModel = m; }
    void makeupDb(Tv db)     { mMakeup = std::pow(Tv(10), db / Tv(20)); }
    void wet(Tv w)           { mWet = std::clamp(w, Tv(0), Tv(1)); }

    void saturation(Tv s)    { mSat = std::max(Tv(0), s); }
    void thd(Tv d)           { mThd = std::max(Tv(0), d); }
    void bleed(Tv b)         { mBleed = std::max(Tv(0), b); }
    void asymmetry(Tv a)     { mAsym = std::clamp(a, Tv(-1), Tv(1)); }

    // ---- NEW: folding controls ----
    // 0 = off, 1 = fully enabled when VCA closes
    void foldAmount(Tv a)        { mFoldAmt = std::clamp(a, Tv(0), Tv(1)); }

    // nominal fold threshold at "fully folded" (smaller = more folding)
    void foldThreshold(Tv th)    { mFoldBaseTh = std::max(Tv(0), th); }

    // perceptual curve for gain-reduction -> fold depth (0.5..1.5 typical)
    void foldCurve(Tv c)         { mFoldCurve = std::max(Tv(0.01), c); }

    // prevents division blowups / denorm storms
    void foldMinThreshold(Tv th) { mFoldMinTh = std::max(Tv(1e-6), th); }

    // ---- state ----
    void reset(){
        mFolder.reset();
    }

    // ---- processing ----
    /// x = input sample
    /// g = linear gain control (>= 0)
    inline Tv operator()(Tv x, Tv g){
        g = std::max(g, Tv(0));

        // Ideal VCA
        Tv yIdeal = x * g * mMakeup;

        // Model-dependent behavior
        Tv satAmt = Tv(0);
        Tv h2 = Tv(0);
        Tv h3 = Tv(0);
        Tv bleedAmt = mBleed;

        switch(mModel){
        case Model::THAT2181:
            satAmt = mSat * Tv(0.8);
            h2 = mThd * Tv(0.15);
            h3 = mThd * Tv(0.6);
            bleedAmt *= Tv(0.3);
            break;

        case Model::DBX:
            satAmt = mSat * Tv(1.2);
            h2 = mThd * Tv(0.5);
            h3 = mThd * Tv(0.5);
            bleedAmt *= Tv(1.0);
            break;

        case Model::SSL:
            satAmt = mSat * Tv(0.6);
            h2 = mThd * Tv(0.1);
            h3 = mThd * Tv(0.9);
            bleedAmt *= Tv(0.5);
            break;

        default:
            break;
        }

        // ------------------------------------------------------------
        // Integrated fold core (gain-reduction driven, bandlimited)
        // ------------------------------------------------------------
        // Compute "gain reduction" amount in [0..1] for typical VCA usage.
        // If g > 1, treat as no fold (you can change this if you want).
        const Tv gr = scl::clip(Tv(1) - g, Tv(0), Tv(1));

        // safe power curve (avoids pow(negative, frac) -> NaN)
        const Tv foldDepth = (gr > Tv(0))
            ? std::exp(mFoldCurve * std::log(gr))
            : Tv(0);

        // fold mix: how much folding replaces saturation
        const Tv foldMix = mFoldAmt * foldDepth;

        Tv y = yIdeal;

        if(foldMix > Tv(0)){
            // Shrink threshold as VCA closes (more GR => lower threshold => more folds)
            // Use a floor to avoid denorm/NaN.
            const Tv th = std::max(
                mFoldBaseTh * (Tv(1) - foldDepth),
                mFoldMinTh
            );

            // scale-invariant fold
            const Tv yFold = mFolder(y / th) * th;

            // IMPORTANT: to avoid "fizz", do NOT stack strong nonlinearities.
            // We MORPH between saturation and folding.
            // We compute saturation on the same pre-THD signal, then crossfade.
            const Tv ySat = saturateNorm(y, satAmt);

            y = (Tv(1) - foldMix) * ySat + foldMix * yFold;

            // When fold is dominant, reduce extra polynomial THD a bit (keeps it musical).
            const Tv thdScale = (Tv(1) - foldMix);
            h2 *= thdScale;
            h3 *= thdScale;
        } else {
            // Normal saturation path when folding is off
            y = saturateNorm(y, satAmt);
        }

        // ------------------------------------------------------------
        // Harmonic coloration (polynomial)
        // ------------------------------------------------------------
        if(h2 > Tv(0) || h3 > Tv(0)){
            const Tv bias = mAsym * Tv(0.2);
            // clamp avoids runaway wideband when someone cranks THD
            const Tv z = std::clamp(y + bias, Tv(-2), Tv(2));
            const Tv z2 = z * z;
            const Tv z3 = z2 * z;
            y += h2 * z2;
            y += h3 * z3;
        }

        // ------------------------------------------------------------
        // Control-path bleed (gain-dependent feedthrough)
        // ------------------------------------------------------------
        y += x * (bleedAmt * (g - Tv(1)));

        // ------------------------------------------------------------
        // Wet/dry (relative to original input)
        // ------------------------------------------------------------
        Tv out = (Tv(1) - mWet) * x + mWet * y;

        // NaN/Inf hardening (poison prevention)
        if(!std::isfinite(double(out))){
            reset();
            return Tv(0);
        }    
        return out;
    }

    /// Gamma domain hook
    void onDomainChange(double r){
        // VCA coefficients are memoryless, but wavefolder is not.
        mFolder.onDomainChange(r);
        (void)r;
    }

private:
    static inline Tv saturateNorm(Tv x, Tv amt){
        if(amt <= Tv(0)) return x;
        const Tv d = Tv(1) + amt;
        return std::tanh(d * x) / d; // unity slope at 0
    }

private:
    Model mModel { Model::Clean };

    Tv mMakeup { Tv(1) };
    Tv mWet    { Tv(1) };

    Tv mSat    { Tv(0) };
    Tv mThd    { Tv(0) };
    Tv mBleed  { Tv(0) };
    Tv mAsym   { Tv(0) };

    // ---- folding params ----
    Tv mFoldAmt    { Tv(0) };   // 0..1
    Tv mFoldBaseTh { Tv(1) };   // nominal threshold at full fold
    Tv mFoldCurve  { Tv(0.7) }; // GR->fold curve
    Tv mFoldMinTh  { Tv(1e-3) };// safety floor

    // ---- internal bandlimited wavefolder ----
    BandlimitedWavefolder<Tv, Td, OS> mFolder;
    
};

} // namespace gam
