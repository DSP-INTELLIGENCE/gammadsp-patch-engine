// AnalogVCA_Thermal.hpp
#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "AnalogVCA.hpp"
#include "AnalogVariationSelfHeat.hpp"


namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class VCAThermalHysteresis : public Td {
public:
    // -------------------------------------------------
    // Core VCA (unchanged API)
    // -------------------------------------------------
    AnalogVCA<Tv, Td> vca;

    // -------------------------------------------------
    // Thermal model
    // -------------------------------------------------
    AnalogVariationSelfHeat<Tv, Td> thermal;

    // -------------------------------------------------
    // Thermal coupling strengths
    // -------------------------------------------------
    Tv kAsym = Tv(0.4);   // bias drift (signed)
    Tv kSat  = Tv(0.6);   // compression growth
    Tv kThd  = Tv(0.5);   // harmonic growth

    // -------------------------------------------------
    // Safety limits
    // -------------------------------------------------
    Tv maxSat = Tv(3);
    Tv maxThd = Tv(2);

    // -------------------------------------------------
    // Lifecycle
    // -------------------------------------------------
    void reset(uint32_t seed = 1)
    {
        vca.reset();
        thermal.reset(seed);
        syncBaseParams();
    }

    /// Call this whenever user parameters change
    void syncBaseParams()
    {
        baseAsym = vca.mAsym;
        baseSat  = vca.mSat;
        baseThd  = vca.mThd;
    }

    // -------------------------------------------------
    // Processing
    // -------------------------------------------------
    inline Tv operator()(Tv x, Tv g)
    {
        // --- first pass: clean VCA output (reference)
        const Tv yClean = vca(x, g);

        // --- thermal drive from dissipated power
        // power ∝ y² (sign-independent, physically correct)
        Tv temp = thermal.process(yClean * yClean);

        // clamp thermal state
        temp = scl::clip(temp, Tv(-1), Tv(1));

        // --- compute effective parameters (non-destructive)
        const Tv effAsym = scl::clip(
            baseAsym + std::tanh(temp) * kAsym,
            Tv(-1), Tv(1)
        );

        const Tv effSat = std::min(
            baseSat + scl::abs(temp) * kSat,
            maxSat
        );

        const Tv effThd = std::min(
            baseThd + scl::abs(temp) * kThd,
            maxThd
        );

        // --- temporarily override VCA parameters
        const Tv oldAsym = vca.mAsym;
        const Tv oldSat  = vca.mSat;
        const Tv oldThd  = vca.mThd;

        vca.mAsym = effAsym;
        vca.mSat  = effSat;
        vca.mThd  = effThd;

        // --- final colored output
        const Tv yThermal = vca(x, g);

        // --- restore baseline parameters
        vca.mAsym = oldAsym;
        vca.mSat  = oldSat;
        vca.mThd  = oldThd;

        return yThermal;
    }

    // -------------------------------------------------
    // Gamma domain hook
    // -------------------------------------------------
    void onDomainChange(double r)
    {
        vca.onDomainChange(r);
        thermal.onDomainChange(r);
    }

private:
    // -------------------------------------------------
    // Cached baseline parameters (no drift!)
    // -------------------------------------------------
    Tv baseAsym { Tv(0) };
    Tv baseSat  { Tv(0) };
    Tv baseThd  { Tv(0) };
};

} // namespace gam
