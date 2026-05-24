#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

// ------------------------------------------------------------
// Even-harmonic coloration tuned for compression sustain
// (same idea as before, but with a controllable amount)
// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class EvenHarmonicColor : public Td {
public:
    /// [0..1]
    void amount(Tv a){ mAmount = std::clamp(a, Tv(0), Tv(1)); }
    Tv  amount() const { return mAmount; }

    void reset(){
        mEnv  = Tv(0);
        mBias = Tv(0);
    }

    inline Tv operator()(Tv x)
    {
        const Tv ax = std::abs(x);
        mEnv += mEnvAlpha * (ax - mEnv);
        mBias += mBiasAlpha * (mEnv - mBias);

        Tv y = x + (mBias * mAmount * mBiasScale);

        // ultra-gentle asymmetry, no sharp corners
        if(y >= Tv(0)) y = softPos(y);
        else          y = softNeg(y);

        return y;
    }

    void onDomainChange(double)
    {
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // compressor-friendly timing
        const Tv envTime  = Tv(0.01);  // 10 ms
        const Tv biasTime = Tv(0.15);  // 150 ms

        mEnvAlpha  = Tv(1) - std::exp(-Tv(1) / (envTime  * fs));
        mBiasAlpha = Tv(1) - std::exp(-Tv(1) / (biasTime * fs));
    }

private:
    static inline Tv softPos(Tv x){
        return x / (Tv(1) + Tv(0.25) * x);
    }
    static inline Tv softNeg(Tv x){
        return x / (Tv(1) + Tv(0.15) * std::abs(x));
    }

    Tv mAmount    { Tv(0) };
    Tv mEnv       { Tv(0) };
    Tv mBias      { Tv(0) };
    Tv mEnvAlpha  { Tv(0) };
    Tv mBiasAlpha { Tv(0) };
    Tv mBiasScale { Tv(0.35) };
};

} // namespace gam
