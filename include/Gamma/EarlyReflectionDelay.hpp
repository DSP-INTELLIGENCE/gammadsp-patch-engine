#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<
        unsigned N = 8,
        class Tv = gam::real,
        template<class> class Si = ipl::Linear,
        class Tp = gam::real,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class EarlyReflectionDelay: public Td {
    public:
        using DelayT = Delay<Tv, Si, Td>;

        EarlyReflectionDelay(){
            for(unsigned i = 0; i < N; ++i){
                mGain[i] = Tp(0);
                mDelaySamp[i] = 0;
            }
        }

        /// Allocate delay buffer (max reflection time)
        void maxDelay(float d){
            mDelay.maxDelay(d);
        }

        /// Set tap delay in seconds
        void tapDelay(unsigned i, float d){
            mDelaySamp[i] = mDelay.delayFToI(d);
        }

        /// Set tap gain
        void tapGain(unsigned i, Tp g){
            mGain[i] = g;
        }

        /// Convenience: set tap
        void tap(unsigned i, float d, Tp g){
            tapDelay(i, d);
            tapGain(i, g);
        }

        /// Reset internal buffer
        void reset(){
            if(mDelay.isSoleOwner())
                mDelay.zero();
        }

        /// Process sample
        Tv operator()(Tv in){
            mDelay.write(in);

            Tv out = Tv(0);
            for(unsigned i = 0; i < N; ++i){
                out += mDelay.read(mDelaySamp[i]) * mGain[i];
            }
            return out;
        }

    private:
        DelayT mDelay;
        uint32_t mDelaySamp[N];
        Tp mGain[N];
    };
}