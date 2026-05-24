#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<
        class Tv = gam::real,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class VibratoDelay
    : public Delay<Tv, ipl::AllPass, Td>
    {
    public:
        using Base = Delay<Tv, ipl::AllPass, Td>;

        /// Default constructor
        VibratoDelay()
        : mRate(5.0),     // Hz
        mDepth(0.002),  // seconds
        mCenter(0.002), // seconds
        mPhase(0)
        {}

        /// Allocate delay buffer
        explicit VibratoDelay(float maxDelay)
        : VibratoDelay()
        {
            Base::maxDelay(maxDelay);
        }

        // ---- parameters ----

        /// LFO rate (Hz)
        void rate(Tp v){ mRate = v; }

        /// Modulation depth (seconds)
        void depth(Tp v){ mDepth = v; }

        /// Center delay (seconds)
        void center(Tp v){ mCenter = v; }

        /// Convenience setter
        void set(Tp rate, Tp depth, Tp center){
            mRate   = rate;
            mDepth  = depth;
            mCenter = center;
        }

        /// Reset delay + LFO
        void reset(){
            if(this->isSoleOwner())
                this->zero();
            this->mIpol.prev = Tv(0); // important for all-pass interpolation
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

            // --- delay ---
            Tv y = Base::read();
            Base::write(x);

            // --- 100% wet output ---
            return y;
        }

    private:
        Tp mRate;     // LFO frequency (Hz)
        Tp mDepth;    // modulation depth (seconds)
        Tp mCenter;   // base delay (seconds)
        Tp mPhase;    // LFO phase [0,1)
    };
}