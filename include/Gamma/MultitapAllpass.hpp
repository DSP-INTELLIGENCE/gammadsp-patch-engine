#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Containers.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<
    unsigned N,
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class MultitapAllpass : public Td {
public:
    MultitapAllpass(){
        for(unsigned i=0; i<N; ++i){
            mG[i] = Tp(0.7);
        }
    }

    /// Allocate all stages (call outside audio thread)
    /// maxDelayUnits is in domain units (seconds in default domain)
    void maxDelay(float maxDelayUnits){
        for(unsigned i=0; i<N; ++i){
            mD[i].maxDelay(maxDelayUnits, false);
            mD[i].delay(0);
            mD[i].zero();
        }
    }

    /// Set stage delay (domain units; seconds in default domain)
    void delay(unsigned stage, float units){
        mD[stage].delay(units);
    }

    /// Set stage gain (stable for |g| < 1)
    void gain(unsigned stage, Tp g){
        mG[stage] = scl::clip(g, Tp(-0.999), Tp(0.999));
    }

    /// Convenience: set stage (delay + gain)
    void set(unsigned stage, float units, Tp g){
        delay(stage, units);
        gain(stage, g);
    }

    void reset(){
        for(unsigned i=0; i<N; ++i){
            mD[i].zero();
        }
    }

    /// Process one sample (cascade)
    Tv operator()(Tv x){
        for(unsigned i=0; i<N; ++i){
            x = allpassStage(mD[i], mG[i], x);
        }
        return x;
    }

private:
    // One delay-based allpass section using a single Delay line:
    // y = xd - g*x
    // write(x + g*y)
    static Tv allpassStage(Delay<Tv,Si,Td>& dly, Tp g, Tv x){
        Tv xd = dly.read();
        Tv y  = xd - x * g;
        dly.write(x + y * g);
        return y;
    }

private:
    Delay<Tv,Si,Td> mD[N];
    Tp              mG[N];
};

} // namespace gam
