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
    class WaveguideDelay:: public Td {
    public:
        /// Forward and backward delay lines
        Delay<Tv, Si, Td> fwd;
        Delay<Tv, Si, Td> bwd;

        /// Reflection coefficients (|R| ≤ 1)
        Tp rLeft  = Tp(-1);   // fixed end
        Tp rRight = Tp(-1);   // fixed end

        WaveguideDelay() {}

        WaveguideDelay(float delay){
            maxDelay(delay);
            setDelay(delay);
        }

        WaveguideDelay(float maxDelay, float delay){
            this->maxDelay(maxDelay);
            setDelay(delay);
        }

        // ---- configuration ----

        /// Set total waveguide length (Hz → pitch)
        void freq(Tp hz){
            setDelay(Tp(0.5) / hz);
        }

        /// Set delay in seconds (half round-trip)
        void setDelay(float d){
            fwd.delay(d);
            bwd.delay(d);
        }

        /// Set maximum delay
        void maxDelay(float d){
            fwd.maxDelay(d);
            bwd.maxDelay(d);
        }

        /// Reset internal state
        void reset(){
            if(fwd.isSoleOwner()) fwd.zero();
            if(bwd.isSoleOwner()) bwd.zero();
        }

        // ---- excitation ----

        /// Inject energy at left boundary
        void exciteLeft(Tv v){
            fwd.write(v);
        }

        /// Inject energy at right boundary
        void exciteRight(Tv v){
            bwd.write(v);
        }

        // ---- processing ----

        Tv operator()(Tv in = Tv(0)){
            // read traveling waves
            Tv f = fwd.read();
            Tv b = bwd.read();

            // boundary reflections
            Tv fNext = in + rLeft  * b;
            Tv bNext =        rRight * f;

            // propagate
            fwd.write(fNext);
            bwd.write(bNext);

            // output is wave sum
            return f + b;
        }
    };
}