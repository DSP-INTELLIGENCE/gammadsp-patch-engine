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
    class KarplusStrongDelay : public Delay<Tv, Si, Td> {
    public:
        using Base = Delay<Tv, Si, Td>;

        /// Default constructor
        KarplusStrongDelay()
        : mFbk(0.98),
        mDamp(0.5)
        {
            mLP.freq(2000); // default loss
        }

        /// Allocate delay buffer
        explicit KarplusStrongDelay(float delay)
        : KarplusStrongDelay()
        {
            Base::maxDelay(delay);
            Base::delay(delay);
        }

        /// Allocate with max delay
        KarplusStrongDelay(float maxDelay, float delay)
        : KarplusStrongDelay()
        {
            Base::maxDelay(maxDelay);
            Base::delay(delay);
        }

        // ---- parameters ----

        /// Set string pitch (Hz)
        void freq(Tp hz){
            Base::delay(Tp(1) / hz);
        }

        /// Set feedback (decay), |v| < 1
        void fbk(Tp v){
            mFbk = scl::clip(v, Tp(-0.999), Tp(0.999));
        }

        /// Set damping amount [0,1]
        /// 0 = bright, 1 = very damped
        void damp(Tp v){
            mDamp = scl::clip(v, Tp(0), Tp(1));
            // map damping to lowpass cutoff
            mLP.freq(100.0 + (1.0 - mDamp) * 8000.0);
        }

        /// Pluck the string (excitation)
        template<class Exciter>
        void pluck(Exciter&& exc){
            for(unsigned i = 0; i < this->size(); ++i)
                (*this)[i] = exc();
        }

        /// Reset delay + filter
        void reset(){
            if(this->isSoleOwner())
                this->zero();
            mLP.reset();
        }

        // ---- processing ----
        Tv operator()(const Tv& x = Tv(0)){
            // read delayed wave
            Tv d = Base::read();

            // loss filter in feedback path
            Tv f = mLP(d);

            // write feedback + excitation
            Base::write(x + f * mFbk);

            // output delayed wave
            return d;
        }

    private:
        Tp mFbk;                    // feedback / decay
        Tp mDamp;                   // damping control
        OnePole<Tv, Tp, Td> mLP;    // loss filter
    };
}