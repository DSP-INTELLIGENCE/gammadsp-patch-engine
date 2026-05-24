#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Filter.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class CrossoverBand : public Td {
public:
    CrossoverBand(){
        cutoff(Tv(1000));
    }

    /// Set crossover frequency (Hz)
    void cutoff(Tv freq){
        mFreq = freq;

        // Linkwitz–Riley = cascaded Butterworth (Q = 0.707)
        lp1.type(LOW_PASS);  lp2.type(LOW_PASS);
        hp1.type(HIGH_PASS); hp2.type(HIGH_PASS);

        lp1.freq(freq); lp2.freq(freq);
        hp1.freq(freq); hp2.freq(freq);

        lp1.res(Tv(0.707)); lp2.res(Tv(0.707));
        hp1.res(Tv(0.707)); hp2.res(Tv(0.707));
    }

    /// Reset internal filter state
    void reset(){
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
    }

    /// Process one sample → low/high outputs
    inline void operator()(Tv x, Tv& low, Tv& high){
        low  = lp2(lp1(x));
        high = hp2(hp1(x));
    }

    /// Convenience: sum should equal original (flat magnitude)
    inline Tv sum(Tv x){
        Tv l, h;
        (*this)(x, l, h);
        return l + h;
    }

    void onDomainChange(double){
        cutoff(mFreq);
    }

private:
    Biquad<Tv,Tv,Td> lp1, lp2;
    Biquad<Tv,Tv,Td> hp1, hp2;

    Tv mFreq { Tv(1000) };
};

} // namespace gam
