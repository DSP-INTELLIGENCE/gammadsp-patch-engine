#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AsymmetryController : public Td {
public:
    /// [-1 .. +1]
    void amount(Tv a){
        mAmount = std::clamp(a, Tv(-1), Tv(1));
    }

    Tv amount() const { return mAmount; }

    void reset(Tv v = Tv(0)){
        mAmountLP = v;
        mBias     = Tv(0);
    }

    /// Process sample
    inline Tv operator()(Tv x){
        // smooth control
        mAmountLP += mAmtAlpha * (mAmount - mAmountLP);

        // bias memory (slower)
        mBias += mBiasAlpha * (mAmountLP - mBias);

        // apply dynamic bias
        Tv y = x + mBias * mBiasScale;

        // asymmetric saturation with unity slope at 0
        if(y >= Tv(0)){
            y = satPos(y);
        } else {
            y = satNeg(y);
        }

        return y;
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // smoothing times (seconds)
        const Tv amtTime  = Tv(0.02);   // 20 ms
        const Tv biasTime = Tv(0.10);   // 100 ms

        mAmtAlpha  = Tv(1) - std::exp(-Tv(1) / (amtTime  * fs));
        mBiasAlpha = Tv(1) - std::exp(-Tv(1) / (biasTime * fs));
    }

private:
    // asymmetrical soft saturation (unity slope at 0)
    static inline Tv satPos(Tv x){
        // x / (1 + x) but slope-corrected
        return x / (Tv(1) + x);
    }

    static inline Tv satNeg(Tv x){
        // gentler on negative side
        return x / (Tv(1) + Tv(0.5) * std::abs(x));
    }

private:
    // params
    Tv mAmount { Tv(0) };

    // smoothed control
    Tv mAmountLP { Tv(0) };

    // dynamic bias
    Tv mBias { Tv(0) };

    // coefficients
    Tv mAmtAlpha  { Tv(0.001) };
    Tv mBiasAlpha { Tv(0.001) };

    // scaling
    Tv mBiasScale { Tv(0.3) };
};

} // namespace gam
