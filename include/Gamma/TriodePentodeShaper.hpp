#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodePentodeShaper : public Td {
public:
    // --------------------------------------------------
    // Controls
    // --------------------------------------------------
    // triode ↔ pentode blend [0..1]
    // 0 = pure triode, 1 = pure pentode
    void mode(Tv v){ mMode = clamp(v); }

    // drive before shaping
    void drive(Tv d){ mDrive = std::max(d, Tv(0)); }

    // output trim
    void trim(Tv t){ mTrim = t; }

    Tv mode() const { return mMode; }

    void reset(){
        mModeLP = mMode;
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // ~30 ms smoothing
        const Tv t = Tv(0.03);
        mSmooth = Tv(1) - std::exp(-Tv(1) / (t * fs));
    }

    // --------------------------------------------------
    // Processing
    // --------------------------------------------------
    inline Tv operator()(Tv x){
        // smooth mode
        mModeLP += mSmooth * (mMode - mModeLP);

        // pre-drive
        Tv xd = x * (Tv(1) + mDrive * Tv(4));

        // triode (asymmetric, soft)
        Tv yTri = triode_(xd);

        // pentode (symmetric, harder)
        Tv yPen = pentode_(xd);

        // blend
        Tv y = lerp(yTri, yPen, mModeLP);

        return y * mTrim;
    }

private:
    // --------------------------------------------------
    // Triode transfer
    // --------------------------------------------------
    // soft, asymmetric, dominant 2nd harmonic
    static Tv triode_(Tv x){
        // asymmetric bias that grows with level
        Tv b = Tv(0.25) * std::tanh(Tv(0.8) * x);

        // soft conduction curve
        return std::tanh(x + b);
    }

    // --------------------------------------------------
    // Pentode transfer
    // --------------------------------------------------
    // symmetric, sharper knee, strong odd harmonics
    static Tv pentode_(Tv x){
        // cubic + saturation gives classic odd stack
        Tv y = x - Tv(0.2) * x * x * x;
        return std::tanh(Tv(1.6) * y);
    }

    // --------------------------------------------------
    static Tv lerp(Tv a, Tv b, Tv t){
        return a + (b - a) * t;
    }

    static Tv clamp(Tv v){
        return std::min(Tv(1), std::max(Tv(0), v));
    }

private:
    // params
    Tv mMode  { Tv(0) };   // 0=triode, 1=pentode
    Tv mDrive { Tv(0.5) };
    Tv mTrim  { Tv(1) };

    // smoothing
    Tv mModeLP { Tv(0) };
    Tv mSmooth { Tv(0.001) };
};

} // namespace gam
