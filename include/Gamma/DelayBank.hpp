#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam 
{
    template<
        unsigned N,
        class Tv = gam::real,
        template<class> class Si = ipl::Linear,
        class Td = GAM_DEFAULT_DOMAIN
    >
    class DelayBank: public Td {
    public:
        using DelayT = Delay<Tv, Si, Td>;

        DelayBank() {}

        /// Allocate max delay for all lines
        void maxDelay(float d){
            for(auto& dl : mDelay)
                dl.maxDelay(d);
        }

        /// Set delay (seconds) for one line
        void delay(unsigned i, float d){
            mDelay[i].delay(d);
        }

        /// Set delays for all lines
        void delays(const std::array<float, N>& d){
            for(unsigned i = 0; i < N; ++i)
                mDelay[i].delay(d[i]);
        }

        /// Reset all delay lines
        void reset(){
            for(auto& dl : mDelay)
                if(dl.isSoleOwner())
                    dl.zero();
        }

        /// Write input to all delay lines
        void write(const Tv& v){
            for(auto& dl : mDelay)
                dl.write(v);
        }

        /// Read from one delay line
        Tv read(unsigned i) const {
            return mDelay[i].read();
        }

        /// Read all delay outputs
        void read(Tv* out) const {
            for(unsigned i = 0; i < N; ++i)
                out[i] = mDelay[i].read();
        }

        /// Access delay line directly
        DelayT& operator[](unsigned i){ return mDelay[i]; }
        const DelayT& operator[](unsigned i) const { return mDelay[i]; }

        static constexpr unsigned size(){ return N; }

    private:
        DelayT mDelay[N];
    };
}