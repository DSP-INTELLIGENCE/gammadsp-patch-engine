#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class HarmonicEnhancer : public Td {
public:
    // --- user control ---
    Tv amount = Tv(0.3);   // 0..1

    void reset(){
        highHP.reset();
        highLP.reset();
        amountLP = amount;
    }

    inline Tv operator()(Tv x){
        // smooth amount
        amountLP += smooth * (amount - amountLP);

        // isolate upper band
        const Tv high = highLP.lp(highHP.hp(x));

        // generate harmonics
        const Tv h = sat(high * Tv(2)) - high;

        // blend back in
        x += h * (amountLP * Tv(0.25));

        return x;
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // HF emphasis band
        highHP.freq(Tv(4000));
        highLP.freq(Tv(16000));

        // ~5 ms smoothing
        smooth = Tv(1) - std::exp(Tv(-1) / (Tv(0.005) * fs));

        reset();
    }

private:
    // --- filters ---
    TPT1Pole<Tv> highHP;
    TPT1Pole<Tv> highLP;

    // --- smoothing ---
    Tv amountLP { Tv(0.3) };
    Tv smooth   { Tv(0.002) };

    // --- core saturation ---
    inline Tv sat(Tv x) const {
        return x / (Tv(1) + std::fabs(x));
    }
};

} // namespace gam
