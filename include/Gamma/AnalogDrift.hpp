#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/Noise.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AnalogDrift : public Td {
public:
    AnalogDrift(
        T depth = T(0.001),     // modulation depth
        T rate  = T(0.05)       // Hz-ish feel (very slow)
    ){
        amount(depth);
        speed(rate);
        reset();
    }

    /// Overall drift depth
    void amount(T v){ mAmount = scl::max(v, T(0)); }

    /// Drift speed (Hz-ish, perceptual)
    void speed(T v){
        mSpeed = scl::max(v, T(1e-4));
        updateCoeff();
    }

    void reset(){
        x1 = x2 = T(0);
    }

    /// Generate drift sample
    T operator()(){
        // White noise in [-1,1]
        T n = noise();

        // Correlated random walk (2-pole smoothing)
        x1 += alpha * (n - x1);
        x2 += beta  * (x1 - x2);

        return x2 * mAmount;
    }

    void onDomainChange(double){
        updateCoeff();
    }

private:
    void updateCoeff(){
        // Time constants derived from speed
        // Slower speed → smaller coefficients
        T tau1 = T(1) / mSpeed;
        T tau2 = T(4) / mSpeed;

        alpha = T(1) - scl::exp(T(-1) / (tau1 * Td::spu()));
        beta  = T(1) - scl::exp(T(-1) / (tau2 * Td::spu()));
    }

    // Noise source
    NoiseWhite<> rng;

    // State
    T x1 = T(0), x2 = T(0);

    // Parameters
    T mAmount = T(0.001);
    T mSpeed  = T(0.05);

    // Coefficients
    T alpha = T(0.001);
    T beta  = T(0.0003);
};

} // namespace gam
