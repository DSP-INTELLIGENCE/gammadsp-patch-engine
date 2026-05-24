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
        class Td = GAM_DEFAULT_DOMAIN
    >
    class FIRDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        struct Tap {
            uint32_t delay; // fixed-point delay
            Tv coef;        // FIR coefficient
            bool active;
        };

        /// Default constructor
        FIRDelay() {}

        /// Allocate delay buffer and taps
        FIRDelay(float maxDelay, unsigned numTaps){
            Base::maxDelay(maxDelay);
            taps(numTaps);
        }

        // ---- taps ----
        void taps(unsigned n){
            mTaps.resize(n);
            for(auto& t : mTaps){
                t.delay  = 0;
                t.coef   = Tv(0);
                t.active = true;
            }
        }

        unsigned taps() const { return mTaps.size(); }

        /// Set tap delay (domain units)
        void delay(float v, unsigned tap){
            mTaps[tap].delay = this->delayFToI(v);
        }

        /// Set tap delay (fractional samples)
        void delaySamplesR(float samples, unsigned tap){
            mTaps[tap].delay = this->delayFToI(samples * Td::ups());
        }

        /// Set tap coefficient
        void coef(const Tv& v, unsigned tap){
            mTaps[tap].coef = v;
        }

        /// Enable / disable tap
        void active(bool v, unsigned tap){
            mTaps[tap].active = v;
        }

        /// Reset delay memory
        void reset(){
            if(this->isSoleOwner())
                this->zero();
        }

        // ---- processing ----
        Tv operator()(const Tv& x){
            Tv y(0);

            // --- READ PHASE ---
            for(const auto& t : mTaps){
                if(!t.active) continue;
                y += this->mIpol(*this, this->mPhase - t.delay) * t.coef;
            }

            // --- WRITE PHASE ---
            Base::write(x);

            return y;
        }

    private:
        std::vector<Tap> mTaps;
    };
}