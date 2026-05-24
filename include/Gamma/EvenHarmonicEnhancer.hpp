#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class EvenHarmonicEnhancer : public Td {
public:
    // -------------------------------------------------
    // User parameters
    // -------------------------------------------------

    /// Overall strength [0..1]
    void amount(Tv a){
        mAmount = scl::clip(a, Tv(0), Tv(1));
    }

    /// Envelope response time (seconds)
    void envelopeTime(Tv s){
        mEnvTime = std::max(s, Tv(1e-4));
        updateCoeffs();
    }

    /// Bias memory time (seconds) – longer = more “analog”
    void memoryTime(Tv s){
        mBiasTime = std::max(s, Tv(1e-4));
        updateCoeffs();
    }

    /// Wet / dry mix
    void mix(Tv m){
        mMix = scl::clip(m, Tv(0), Tv(1));
    }

    // -------------------------------------------------
    // Lifecycle
    // -------------------------------------------------
    void reset(){
        mEnv  = Tv(0);
        mBias = Tv(0);
        mX1   = mY1 = Tv(0);
    }

    // -------------------------------------------------
    // Processing
    // -------------------------------------------------
    inline Tv operator()(Tv x)
    {
        // ---- envelope follower (RMS-ish, rectified)
        const Tv ax = std::abs(x);
        mEnv += mEnvCoef * (ax - mEnv);

        // ---- bias memory (slow hysteresis)
        mBias += mBiasCoef * (mEnv - mBias);

        // ---- inject bias (even-harmonic source)
        Tv y = x + mBias * mAmount * mBiasScale;

        // ---- ultra-gentle asymmetric soft curve
        if(y >= Tv(0)){
            y = softPos(y);
        } else {
            y = softNeg(y);
        }

        // ---- DC blocker (bias creates DC)
        y = dcBlock(y);

        // ---- wet / dry
        return (Tv(1) - mMix) * x + mMix * y;
    }

    // -------------------------------------------------
    // Gamma domain hook
    // -------------------------------------------------
    void onDomainChange(double)
    {
        updateCoeffs();
    }

private:
    // -------------------------------------------------
    // Coefficient update
    // -------------------------------------------------
    void updateCoeffs()
    {
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        mEnvCoef  = Tv(1) - std::exp(-Tv(1) / (mEnvTime  * fs));
        mBiasCoef = Tv(1) - std::exp(-Tv(1) / (mBiasTime * fs));

        // DC blocker (~10 Hz)
        const Tv fc = Tv(10);
        mDcR = std::exp(-Tv(6.283185307179586) * fc / fs);
    }

    // -------------------------------------------------
    // Asymmetric soft curves (unity slope at 0)
    // -------------------------------------------------
    static inline Tv softPos(Tv x)
    {
        // very gentle curvature
        return x / (Tv(1) + Tv(0.25) * x);
    }

    static inline Tv softNeg(Tv x)
    {
        // negative side slightly looser
        return x / (Tv(1) + Tv(0.15) * std::abs(x));
    }

    // -------------------------------------------------
    // 1-pole DC blocker
    // -------------------------------------------------
    inline Tv dcBlock(Tv x)
    {
        const Tv y = x - mX1 + mDcR * mY1;
        mX1 = x;
        mY1 = y;
        return y;
    }

private:
    // -------------------------------------------------
    // State
    // -------------------------------------------------
    Tv mEnv  { Tv(0) };
    Tv mBias { Tv(0) };

    // DC blocker state
    Tv mX1 { Tv(0) };
    Tv mY1 { Tv(0) };

    // -------------------------------------------------
    // Parameters
    // -------------------------------------------------
    Tv mAmount   { Tv(0.5) };
    Tv mMix      { Tv(1) };

    Tv mEnvTime  { Tv(0.01) };  // 10 ms
    Tv mBiasTime { Tv(0.15) };  // 150 ms

    // -------------------------------------------------
    // Coefficients
    // -------------------------------------------------
    Tv mEnvCoef  { Tv(0) };
    Tv mBiasCoef { Tv(0) };
    Tv mDcR      { Tv(0) };

    // -------------------------------------------------
    // Tuning
    // -------------------------------------------------
    Tv mBiasScale { Tv(0.35) };
};

} // namespace gam
