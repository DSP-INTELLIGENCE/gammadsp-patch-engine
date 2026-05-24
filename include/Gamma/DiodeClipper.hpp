#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DiodeClipper : public Td {
public:
    DiodeClipper() = default;

    // ------------------------------------------------------------
    // Parameters (instantaneous, no smoothing)
    // ------------------------------------------------------------
    void drive(Tv v)      { mDrive  = std::max(v, Tv(0)); }
    void threshold(Tv v)  { mThresh = std::max(v, Tv(1e-9)); }
    void asymmetry(Tv v)  { mAsym   = v; }
    void trim(Tv v)       { mTrim   = v; }

    // ------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------
    void reset(){}

    void onDomainChange(double){}

    // ------------------------------------------------------------
    // Processing
    // ------------------------------------------------------------
    inline Tv operator()(Tv x){
        // pre-gain
        Tv y = x * mDrive;

        // diode nonlinearity
        y = diode(y);

        // output trim + loudness compensation
        y *= mTrim * loudComp();

        return y;
    }

private:
    // ------------------------------------------------------------
    // Diode transfer (pure function)
    // ------------------------------------------------------------
    inline Tv diode(Tv x) const {
        const Tv a = mThresh;

        Tv y = x;

        // soft diode knee
        if (x > a){
            const Tv d = x - a;
            y = a + d / (Tv(1) + d * d);
        }
        else if (x < -a){
            const Tv d = x + a;
            y = -a + d / (Tv(1) + d * d);
        }

        // asymmetric rectifier flavor (even harmonics)
        // NOTE: exact, no memory
        y += mAsym * y * y;

        return y;
    }

    inline Tv loudComp() const {
        // purely instantaneous loudness compensation
        return Tv(1) / (Tv(1) + Tv(0.35) * mDrive);
    }

private:
    // ------------------------------------------------------------
    // Parameters
    // ------------------------------------------------------------
    Tv mDrive  { Tv(1) };
    Tv mThresh { Tv(0.6) };
    Tv mAsym   { Tv(0) };
    Tv mTrim   { Tv(1) };
};

} // namespace gam
