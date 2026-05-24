#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class AllpassDelay : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    /// Default constructor (no allocation)
    AllpassDelay() : mG(Tp(0)) {}

    /// Allocate buffer for delay
    AllpassDelay(float delay, const Tp& g = Tp(0))
    : Base(delay), mG(g) {}

    /// Allocate buffer for maxDelay
    AllpassDelay(float maxDelay, float delay, const Tp& g = Tp(0))
    : Base(maxDelay, delay), mG(g) {}

    /// Convenience setter
    void set(float delay, const Tp& g_){
        Base::delay(delay);
        g(g_);
    }

    /// Reset delay state safely
    void reset(){
        if(this->isSoleOwner())
            this->zero();
    }

    /// Get feedback coefficient
    Tp g() const { return mG; }

    /// Set feedback coefficient (|g| < 1)
    void g(const Tp& v){
        mG = scl::clip(v, Tp(-0.999), Tp(0.999));
    }

    /// Schroeder all-pass processing
    Tv operator()(const Tv& x){
        const Tv wd = Base::read();      // w[n-m]
        const Tv w  = x + wd * mG;       // w[n]
        const Tv y  = wd - w * mG;       // y[n]
        Base::write(w);
        return y;
    }

private:
    Tp mG;
};

} // namespace gam
