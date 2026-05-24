#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "Gamma/VCAThermalHysteresis.hpp"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AnalogCompressorCell : public Td {
public:
    // --- VCA with thermal memory ---
    VCAThermalHysteresis<Tv, Td> vca;

    // --- compressor parameters ---
    Tv thresholdDb = Tv(-18);
    Tv ratio       = Tv(4);   // >= 1
    Tv makeupDb    = Tv(0);

    // --- detector time constants (Hz) ---
    Tv attackBW  = Tv(10);    // > 0
    Tv releaseBW = Tv(1);     // > 0

    // --- state ---
    Tv env  = Tv(0);
    Tv gain = Tv(1);

    // --- cached coefficients ---
    Tv atkK = Tv(0);
    Tv relK = Tv(0);

    AnalogCompressorCell() { onDomainChange(1.0); }

    void reset(uint32_t seed = 1)
    {
        env  = Tv(0);
        gain = Tv(1);
        vca.reset(seed);
    }

    /// Gamma domain hook
    void onDomainChange(double r)
    {
        // Parameter safety
        const Tv atk = std::max(attackBW,  Tv(1e-6));
        const Tv rel = std::max(releaseBW, Tv(1e-6));

        atkK = Tv(1) - std::exp(-Td::ups() * atk);
        relK = Tv(1) - std::exp(-Td::ups() * rel);

        // Forward domain change to child
        vca.onDomainChange(r);
    }

    inline Tv operator()(Tv x)
    {
        // --- detector (feed-forward; stable and predictable)
        const Tv level = scl::abs(x);

        // --- envelope follower (attack / release)
        const Tv k = (level > env) ? atkK : relK;
        env += k * (level - env);

        // --- gain computer (log domain)
        const Tv r = std::max(ratio, Tv(1));     // clamp ratio
        const Tv envDb = Tv(20) * std::log10(env + Tv(1e-9));
        const Tv over  = envDb - thresholdDb;

        if(over > Tv(0)){
            const Tv gainDb = -over * (Tv(1) - Tv(1) / r);
            gain = std::pow(Tv(10), gainDb / Tv(20));
        } else {
            gain = Tv(1);
        }

        // --- makeup
        const Tv makeup = std::pow(Tv(10), makeupDb / Tv(20));

        // --- thermal VCA (audio path coloration + slow memory)
        return vca(x, gain * makeup);
    }
};

} // namespace gam
