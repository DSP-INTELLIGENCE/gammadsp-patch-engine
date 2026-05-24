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
    class BandpassComb : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        /// Default constructor
        BandpassComb()
        : mFbk(0)
        {}

        /// Allocate delay buffer
        BandpassComb(float delay,
                    Tp fbk,
                    Tp freq,
                    Tp width)
        : Base(delay),
        mFbk(fbk),
        mBP(freq, width)
        {}

        /// Allocate with max delay
        BandpassComb(float maxDelay,
                    float delay,
                    Tp fbk,
                    Tp freq,
                    Tp width)
        : Base(maxDelay, delay),
        mFbk(fbk),
        mBP(freq, width)
        {}

        // ---- parameters ----
        void fbk(Tp v){ mFbk = v; }
        Tp   fbk() const { return mFbk; }

        void freq(Tp v){ mBP.freq(v); }
        Tp   freq() const { return mBP.freq(); }

        void width(Tp v){ mBP.width(v); }

        void set(float delay, Tp fbk, Tp freq, Tp width){
            this->delay(delay);
            mFbk = fbk;
            mBP.set(freq, width);
        }

        void reset(){
            if(this->isSoleOwner())
                this->zero();
            mBP.reset();
        }

        // ---- processing ----
        Tv operator()(const Tv& x){
            // Read delayed sample
            const Tv d = Base::read();

            // Bandpass-filtered feedback
            const Tv fb = mBP(d) * mFbk;

            // Write feedback + input
            Base::write(x + fb);

            // Output is delayed signal
            return d;
        }

    private:
        Tp mFbk;
        Reson<Tv, Tp, Td> mBP;
    };
}