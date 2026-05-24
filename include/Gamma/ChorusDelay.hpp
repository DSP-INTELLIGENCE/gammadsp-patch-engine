#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

    template<
        class Tv = gam::real,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class ChorusDelay
    : public Delay<Tv, ipl::AllPass, Td>
    {
    public:
        using Base = Delay<Tv, ipl::AllPass, Td>;

        /// Default constructor
        ChorusDelay()
        : mRate(0.3), mDepth(0.005), mCenter(0.015),
        mMix(0.5), mPhase(0)
        {
            Base::maxDelay(2.0);
        }

        /// Allocate delay buffer
        ChorusDelay(float maxDelay)
        : ChorusDelay()
        {
            Base::maxDelay(maxDelay);
        }

        // ---- parameters ----

        /// LFO rate in Hz
        void rate(Tp v){ mRate = v; }

        /// Modulation depth in seconds
        void depth(Tp v){ mDepth = v; }

        /// Center delay in seconds
        void center(Tp v){ mCenter = v; }

        /// Dry / wet mix [0,1]
        void mix(Tp v){ mMix = scl::clip(v, Tp(0), Tp(1)); }

        /// Convenience setter
        void set(Tp rate, Tp depth, Tp center, Tp mix){
            mRate   = rate;
            mDepth  = depth;
            mCenter = center;
            mMix    = mix;
        }

        /// Reset delay + interpolator
        void reset(){
            if(this->isSoleOwner())
                this->zero();
            this->mIpol.prev = Tv(0);
            mPhase = Tp(0);
        }

        // ---- processing ----
        Tv operator()(const Tv& x){
            // --- LFO ---
            Tp lfo = std::sin(mPhase * Tp(M_2PI));
            mPhase += mRate * Td::ups();
            if(mPhase >= Tp(1)) mPhase -= Tp(1);

            // --- modulated delay ---
            Tp d = mCenter + mDepth * lfo;
            Base::delay(d);

            // --- delay read/write ---
            Tv y = Base::read();
            Base::write(x);

            // --- dry/wet mix ---
            return x * (Tp(1) - mMix) + y * mMix;
        }

    private:
        Tp mRate;     // LFO frequency (Hz)
        Tp mDepth;    // modulation depth (seconds)
        Tp mCenter;   // base delay (seconds)
        Tp mMix;      // dry/wet
        Tp mPhase;    // LFO phase [0,1)
    };

}