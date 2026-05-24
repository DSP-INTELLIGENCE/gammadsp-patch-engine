#pragma once
#include <algorithm>
#include <cmath>
#include <random>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class FinalOutputStage : public Td {
public:
    // --- user controls ---
    Tv clipAmount   = Tv(0.5);   // 0..1
    Tv ditherAmount = Tv(1.0);   // in LSBs (post-quantization)

    void reset(){
        clipLP = clipAmount;
    }

    inline Tv operator()(Tv x){
        // smooth clip control
        clipLP += clipSmooth * (clipAmount - clipLP);

        // soft clip
        const Tv drive = Tv(1) + clipLP * Tv(4);
        x = softClip(x * drive);

        // TPDF dither (assumes later quantization)
        x += tpdf() * ditherScale;

        return x;
    }

    void onDomainChange(double){
        // control smoothing (~5 ms)
        const Tv fs = Tv(Td::spu());
        if(fs > Tv(0)){
            clipSmooth = Tv(1) - std::exp(Tv(-1) / (Tv(0.005) * fs));
        }

        // default: 16-bit LSB unless user overrides
        ditherScale = ditherAmount * (Tv(1) / Tv(32768));

        reset();
    }

private:
    // --- state ---
    Tv clipLP { Tv(0.5) };
    Tv clipSmooth { Tv(0.002) };

    // --- dither ---
    std::mt19937 rng { 0x12345678 };
    std::uniform_real_distribution<Tv> uni { Tv(-1), Tv(1) };
    Tv ditherScale { Tv(1) / Tv(32768) };

    inline Tv softClip(Tv x) const {
        return x / (Tv(1) + std::fabs(x));
    }

    inline Tv tpdf(){
        // triangular PDF in [-1, 1]
        return (uni(rng) + uni(rng)) * Tv(0.5);
    }
};

} // namespace gam
