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
    class DopplerDelay
    : public Delay<Tv, ipl::AllPass, Td>
    {
    public:
        using Base = Delay<Tv, ipl::AllPass, Td>;

        /// Speed of sound (m/s)
        static constexpr Tp SpeedOfSound = Tp(343);

        /// Default constructor
        DopplerDelay()
        : mDelay(0.01),   // seconds
        mVelocity(0),   // m/s
        mMinDelay(0.0001),
        mMaxDelay(0.1)
        {}

        /// Allocate delay buffer
        explicit DopplerDelay(float maxDelay)
        : DopplerDelay()
        {
            Base::maxDelay(maxDelay);
            mMaxDelay = maxDelay;
        }

        // ---- parameters ----

        /// Set current propagation delay (seconds)
        void delay(Tp v){
            mDelay = scl::clip(v, mMinDelay, mMaxDelay);
        }

        /// Set relative velocity (m/s)
        /// Positive = approaching, Negative = receding
        void velocity(Tp v){ mVelocity = v; }

        /// Clamp delay range
        void range(Tp minDelay, Tp maxDelay){
            mMinDelay = minDelay;
            mMaxDelay = maxDelay;
        }

        /// Reset delay line
        void reset(){
            if(this->isSoleOwner())
                this->zero();
            this->mIpol.prev = Tv(0);
        }

        // ---- processing ----
        Tv operator()(const Tv& x){
            // --- integrate delay from velocity ---
            Tp dt = Td::ups();
            mDelay -= (mVelocity / SpeedOfSound) * dt;

            // Clamp physical delay
            mDelay = scl::clip(mDelay, mMinDelay, mMaxDelay);

            // Apply delay
            Base::delay(mDelay);

            // Delay line
            Tv y = Base::read();
            Base::write(x);

            return y;
        }

    private:
        Tp mDelay;     // current delay (seconds)
        Tp mVelocity;  // relative velocity (m/s)
        Tp mMinDelay;
        Tp mMaxDelay;
    };
}