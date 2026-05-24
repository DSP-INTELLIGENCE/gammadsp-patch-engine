#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DynamicSaturation : public Td {
public:
    /// User control [0..1]
    void amount(Tv a){
        mAmount = std::clamp(a, Tv(0), Tv(1));
    }

    Tv amount() const { return mAmount; }

    void reset(){
        mAmountLP = mAmount;
        mEnv = Tv(0);
    }

    /// Process sample
    inline Tv operator()(Tv x){
        // smooth amount
        mAmountLP += mAmtAlpha * (mAmount - mAmountLP);

        // envelope follower (rectified, one-pole)
        const Tv a = std::abs(x);
        mEnv += mEnvAlpha * (a - mEnv);

        // dynamic drive (soft-bounded)
        const Tv drive = Tv(1) + mAmountLP * mDriveScale * softBound(mEnv);

        // saturation with unity small-signal gain
        Tv y = satNorm(x * drive);

        // preserve body (parallel blend)
        const Tv blend = mAmountLP * Tv(0.6);
        return x * (Tv(1) - blend) + y * blend;
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // smoothing times (seconds)
        const Tv amtTime = Tv(0.05);  // amount smoothing
        const Tv envTime = Tv(0.01);  // envelope response

        mAmtAlpha = Tv(1) - std::exp(-Tv(1) / (amtTime * fs));
        mEnvAlpha = Tv(1) - std::exp(-Tv(1) / (envTime * fs));
    }

private:
    // saturation with unity slope at 0
    static inline Tv satNorm(Tv x){
        return std::tanh(x);
    }

    // soft bounding for envelope → prevents runaway drive
    static inline Tv softBound(Tv x){
        return x / (Tv(1) + x);
    }

private:
    // parameters
    Tv mAmount { Tv(0.5) };

    // smoothed controls
    Tv mAmountLP { Tv(0.5) };

    // envelope
    Tv mEnv { Tv(0) };

    // coefficients
    Tv mAmtAlpha { Tv(0.001) };
    Tv mEnvAlpha { Tv(0.01) };

    // tuning
    Tv mDriveScale { Tv(8) };
};

} // namespace gam
