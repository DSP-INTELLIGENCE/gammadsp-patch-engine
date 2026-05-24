#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"
#include <cmath>

namespace gam
{
    template<
        class Tv = gam::real,
        template<class> class Si = ipl::Linear,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class ZeroDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        /// Default constructor
        ZeroDelay() {}

        /// Allocate buffer for delay
        explicit ZeroDelay(float delay)
        : Base(delay) {}

        /// Allocate with max delay
        ZeroDelay(float maxDelay, float delay)
        : Base(maxDelay, delay) {}

        /// True zero-delay processing
        Tv operator()(const Tv& x){
            Base::write(x);     // write first
            return Base::read();
        }

        /// Read-only tap (unchanged)
        Tv operator()() const {
            return Base::read();
        }
    };
}