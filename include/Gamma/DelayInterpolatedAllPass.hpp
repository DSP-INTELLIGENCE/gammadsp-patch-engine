#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <cassert>
#include <cmath>
#include <cstddef>
#include <array>
#include <algorithm>
#include <vector>
#include <memory>
#include <complex>
#include <random>
#include <stdexcept>

namespace gam
{
    template<
        class Tv = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class DelayInterpolatedAllPass
    : public Delay<Tv, ipl::AllPass, Td>
    {
    public:
        using Base = Delay<Tv, ipl::AllPass, Td>;

        /// Default constructor (no allocation)
        DelayInterpolatedAllPass() {}

        /// Allocate buffer for max delay (domain units)
        explicit DelayInterpolatedAllPass(float maxDelay){
            Base::maxDelay(maxDelay);
        }

        /// Allocate buffer and set initial delay
        DelayInterpolatedAllPass(float maxDelay, float delay){
            Base::maxDelay(maxDelay);
            Base::delay(delay);
        }

        /// Set delay in domain units (may be modulated per-sample)
        void delay(float v){
            Base::delay(v);
        }

        /// Set delay in fractional samples (preferred for modulation)
        void delaySamplesR(float samples){
            Base::delaySamplesR(samples);
        }

        /// Reset delay line AND interpolator state
        void reset(){
            if(this->isSoleOwner())
                this->zero();
            this->mIpol.prev = Tv(0);
        }

        /// Process sample
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
}