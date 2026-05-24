#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class Triode : public Td {
public:
    // --- physical parameters ---
    Tv mu  = Tv(100);   // amplification factor
    Tv kg1 = Tv(1060);  // transconductance scaling
    Tv kp  = Tv(600);   // plate resistance factor
    Tv ex  = Tv(1.4);   // space-charge exponent
    Tv vct = Tv(0.0);   // cutoff / bias voltage

    // non-physical but musical
    Tv driveSat = Tv(0.7);   // soft saturation in control domain
    Tv iMax     = Tv(10);    // max plate current clamp

    void reset() {}

    /// Plate current from grid-cathode and plate-cathode voltages
    inline Tv operator()(Tv vgk, Tv vpk) const
    {
        // Control domain (grid + plate feedback)
        Tv e = vgk + vct + vpk / mu;

        // Smooth cutoff (no conduction below cutoff)
        e = std::max(e, Tv(0));

        // Soft saturation of drive domain
        e = std::tanh(driveSat * e);

        // Avoid denormals / pow(0)
        e = std::max(e, Tv(1e-6));

        // Plate resistance interaction
        const Tv denom = Tv(1) + kp * std::max(vpk, Tv(0.05));

        // Space-charge limited plate current
        Tv ip = kg1 * std::pow(e, ex) / denom;

        // Physical clamp (never negative, never runaway)
        return std::clamp(ip, Tv(0), iMax);
    }

    void onDomainChange(double) {
        // purely static model; nothing to recompute
    }
};

} // namespace gam
