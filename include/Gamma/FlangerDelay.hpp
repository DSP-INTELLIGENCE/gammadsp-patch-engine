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
        template<class> class Si = ipl::Linear,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class FlangerDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        /// Default constructor
        FlangerDelay()
        : mRate(0.2),
        mDepth(0.002),
        mCenter(0.001),
        mMix(0.5),
        mFbk(0.5),
        mPhase(0)
        {}

        /// Allocate delay buffer
        explicit FlangerDelay(float maxDelay)
        : FlangerDelay()
        {
            Base::maxDelay(maxDelay);
        }

        // ---- parameters ----

        /// LFO rate in Hz
        void rate(Tp v){ mRate = v; }

        /// Modulation depth (seconds)
        void depth(Tp v){ mDepth = v; }

        /// Center delay (seconds)
        void center(Tp v){ mCenter = v; }

        /// Feedback amount (|v| < 1)
        void fbk(Tp v){ mFbk = scl::clip(v, Tp(-0.99), Tp(0.99)); }

        /// Dry/wet mix [0,1]
        void mix(Tp v){ mMix = scl::clip(v, Tp(0), Tp(1)); }

        /// Convenience setter
        void set(Tp rate, Tp depth, Tp center, Tp fbk, Tp mix){
            mRate   = rate;
            mDepth  = depth;
            mCenter = center;
            mFbk    = fbk;
            mMix    = mix;
        }

        /// Reset delay and LFO
        void reset(){
            if(this->isSoleOwner())
                this->zero();
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

            // --- delay read ---
            Tv delayed = Base::read();

            // --- feedback write ---
            Base::write(x + delayed * mFbk);

            // --- mix ---
            return x * (Tp(1) - mMix) + delayed * mMix;
        }

    private:
        Tp mRate;     // LFO rate (Hz)
        Tp mDepth;    // modulation depth (seconds)
        Tp mCenter;   // base delay (seconds)
        Tp mMix;      // dry/wet
        Tp mFbk;      // feedback
        Tp mPhase;    // LFO phase [0,1)
    };
}