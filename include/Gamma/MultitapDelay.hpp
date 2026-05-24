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
    class MultitapDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        struct Tap {
            uint32_t delay; // fixed-point delay
            Tv gain;
            bool active;
        };

        /// Default constructor
        MultitapDelay() {}

        /// Allocate buffer and taps
        MultitapDelay(float maxDelay, unsigned numTaps){
            Base::maxDelay(maxDelay);
            taps(numTaps);
        }

        /// Set number of taps
        void taps(unsigned n){
            mTaps.resize(n);
            for(auto& t : mTaps){
                t.delay  = 0;
                t.gain   = Tv(1);
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

        /// Set tap gain
        void gain(Tv v, unsigned tap){
            mTaps[tap].gain = v;
        }

        /// Enable / disable tap
        void active(bool v, unsigned tap){
            mTaps[tap].active = v;
        }

        /// Read a single tap
        Tv read(unsigned tap) const {
            const auto& t = mTaps[tap];
            if(!t.active) return Tv(0);
            return this->mIpol(*this, this->mPhase - t.delay) * t.gain;
        }

        /// Sum all taps
        Tv read(){
            Tv sum(0);
            for(const auto& t : mTaps){
                if(t.active){
                    sum += this->mIpol(*this, this->mPhase - t.delay) * t.gain;
                }
            }
            return sum;
        }

        /// Process sample (write + read)
        Tv operator()(const Tv& x){
            Tv y = read();
            Base::write(x);
            return y;
        }

    private:
        std::vector<Tap> mTaps;
    };
}