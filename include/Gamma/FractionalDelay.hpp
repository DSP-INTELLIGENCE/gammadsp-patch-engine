#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Td = GAM_DEFAULT_DOMAIN
>
class FractionalDelay : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    /// Default constructor (no allocation)
    FractionalDelay() = default;

    /// Allocate buffer for max delay (fractional samples)
    explicit FractionalDelay(float maxDelaySamples){
        maxDelaySamplesR(maxDelaySamples);
    }

    /// Allocate buffer and set initial delay (fractional samples)
    FractionalDelay(float maxDelaySamples, float delaySamples){
        maxDelaySamplesR(maxDelaySamples);
        delaySamplesR(delaySamples);
    }

    /// Set delay in *fractional samples*
    void delaySamplesR(float samples){
        Base::delaySamplesR(samples);
    }

    /// Get delay in *fractional samples*
    float delaySamplesR() const {
        return Base::delaySamplesR();
    }

    /// Set maximum delay in *fractional samples*
    void maxDelaySamplesR(float samples){
        // Convert samples -> domain units
        Base::maxDelay(samples * Td::ups());
    }

    /// Process input sample (read then write)
    Tv operator()(const Tv& x){
        Tv y = Base::read();
        Base::write(x);
        return y;
    }

    /// Read-only tap
    Tv operator()() const {
        return Base::read();
    }
};

} // namespace gam
