#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
struct AnalogVariationSelfHeat : public Td
{
    // --- static component tolerances
    T cutoffTol = T(1);
    T resTol    = T(1);
    T driveTol  = T(1);

    // --- thermal state
    T temp  = T(0);

    // --- stochastic drift
    T rw    = T(0);

    // --- signal power
    T power = T(0);

    // --- controls (time-domain)
    T heatGain = T(0.02);   // heating strength
    T heatBW   = T(1.0);    // heating speed (Hz)
    T coolBW   = T(0.05);   // cooling speed (Hz)
    T driftBW  = T(0.005);  // thermal noise bandwidth (Hz)

    // --- saturation
    T tSat = T(0.6);        // heating efficiency rolloff
    T tMax = T(1.2);        // absolute thermal headroom

    // --- cached coefficients (SR dependent)
    T pCoef     = T(0);
    T driftLeak = T(0);
    T driftDiff = T(0);
    T heatK     = T(0);
    T coolK     = T(0);

    // RNG
    uint32_t rng = 1;

    AnalogVariationSelfHeat()
    {
        onDomainChange(1.0);
        reset(1);
    }

    void reset(uint32_t seed = 1)
    {
        rng = seed;
        temp = rw = power = T(0);

        cutoffTol = T(1) + uniform(-T(0.015), T(0.015));
        resTol    = T(1) + uniform(-T(0.020), T(0.020));
        driveTol  = T(1) + uniform(-T(0.020), T(0.020));
    }

    /// Gamma domain callback
    void onDomainChange(double /*r*/)
    {
        const T fs = T(Td::spu());      // samples per second (Gamma style)
        if(fs <= T(0)) return;

        const T dt = T(1) / fs;         // seconds per sample

        // --- power envelope (~5 Hz)
        pCoef = T(1) - std::exp(-dt * T(5.0));

        // --- OU drift
        driftLeak = std::exp(-dt * driftBW);
        driftDiff = std::sqrt(std::max(T(0), T(1) - driftLeak * driftLeak));

        // --- hysteresis coefficients
        heatK = T(1) - std::exp(-dt * heatBW);
        coolK = T(1) - std::exp(-dt * coolBW);
    }

    /// Call once per sample with signal value
    inline T process(T signal)
    {
        // --- RMS-ish power
        const T s2 = signal * signal;
        power += pCoef * (s2 - power);

        // --- stochastic thermal wander
        rw = driftLeak * rw + driftDiff * white();

        // --- heating efficiency saturation
        const T ts  = (tSat > T(0)) ? tSat : T(1);
        const T u   = temp / ts;
        const T eff = T(1) / (T(1) + u * u);   // smooth rolloff

        // --- target temperature
        const T target = heatGain * eff * power + rw;

        // --- hysteretic integration
        const T k = (target > temp) ? heatK : coolK;
        temp += k * (target - temp);

        // --- thermal headroom saturation (soft)
        if(tMax > T(0))
            temp = tMax * std::tanh(temp / tMax);

        return temp;
    }

    inline T operator()(T input) { return process(input); }

private:
    inline T white()
    {
        // xorshift32
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;

        // Convert to approx [-1, 1)
        // int32_t(rng) spans full signed range; scale by 1/2^31
        return T(int32_t(rng)) * T(4.656612873e-10); // 1 / 2^31
    }

    inline T uniform(T a, T b)
    {
        // white() is ~[-1,1), map to [0,1)
        const T u01 = white() * T(0.5) + T(0.5);
        return a + (b - a) * u01;
    }
};

} // namespace gam
