// AnalogVCA.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AnalogVCA : public Td {
public:
    enum class Model { Clean, THAT2181, DBX, SSL };

    // ---- parameters ----
    void model(Model m)      { mModel = m; }
    void makeupDb(Tv db)     { mMakeup = std::pow(Tv(10), db / Tv(20)); }
    void wet(Tv w)           { mWet = std::clamp(w, Tv(0), Tv(1)); }

    void saturation(Tv s)    { mSat = std::max(Tv(0), s); }
    void thd(Tv d)           { mThd = std::max(Tv(0), d); }
    void bleed(Tv b)         { mBleed = std::max(Tv(0), b); }
    void asymmetry(Tv a)     { mAsym = std::clamp(a, Tv(-1), Tv(1)); }

    // ---- state ----
    void reset(){
        // VCA itself is memoryless, but keep this for Gamma consistency
    }

    // ---- processing ----
    /// x = input sample
    /// g = linear gain control (>= 0)
    inline Tv operator()(Tv x, Tv g){
        g = std::max(g, Tv(0));

        // Ideal VCA
        Tv y = x * g * mMakeup;

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

        // Saturation (drive-normalized tanh)
        y = saturateNorm(y, satAmt);

        // Harmonic coloration (polynomial)
        if(h2 > Tv(0) || h3 > Tv(0)){
            const Tv bias = mAsym * Tv(0.2);
            const Tv z = y + bias;
            const Tv z2 = z * z;
            const Tv z3 = z2 * z;
            y += h2 * z2;
            y += h3 * z3;
        }

        // Control-path bleed (gain-dependent feedthrough)
        y += x * (bleedAmt * (g - Tv(1)));

        // Wet/dry
        return (Tv(1) - mWet) * x + mWet * y;
    }

    /// Gamma domain hook
    void onDomainChange(double){
        // No time-dependent coefficients here,
        // but kept for Gamma consistency & future extensions
    }


    static inline Tv saturateNorm(Tv x, Tv amt){
        if(amt <= Tv(0)) return x;
        const Tv d = Tv(1) + amt;
        return std::tanh(d * x) / d; // unity slope at 0
    }

    Model mModel { Model::Clean };

    Tv mMakeup { Tv(1) };
    Tv mWet    { Tv(1) };

    Tv mSat    { Tv(0) };
    Tv mThd    { Tv(0) };
    Tv mBleed  { Tv(0) };
    Tv mAsym   { Tv(0) };
};

} // namespace gam
