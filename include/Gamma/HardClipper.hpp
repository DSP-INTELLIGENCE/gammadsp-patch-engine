#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class HardClipper : public Td {
public:
    // --- user controls ---
    Tv threshold = Tv(1);   // clipping point
    Tv drive     = Tv(1);   // input drive
    Tv asym      = Tv(0);   // even harmonic bias
    Tv trim      = Tv(1);   // output trim

    void reset(){
        thrLP  = threshold;
        drvLP  = drive;
        asymLP = asym;
    }

    inline Tv operator()(Tv x){
        // smooth controls
        thrLP  += smooth * (threshold - thrLP);
        drvLP  += smooth * (drive     - drvLP);
        asymLP += smooth * (asym      - asymLP);

        // drive in
        Tv y = x * drvLP;

        // clip
        y = clipCore(y);

        // output trim & loudness compensation
        y *= trim * loudComp();

        return y;
    }

    void onDomainChange(double){
        // ~5 ms smoothing
        const Tv fs = Tv(Td::spu());
        if(fs > Tv(0)){
            smooth = Tv(1) - std::exp(Tv(-1) / (Tv(0.005) * fs));
        }
        reset();
    }

private:
    // --- smoothed params ---
    Tv thrLP  { Tv(1) };
    Tv drvLP  { Tv(1) };
    Tv asymLP { Tv(0) };

    Tv smooth { Tv(0.002) };

    // --- core ---
    inline Tv clipCore(Tv x) const {
        const Tv t = thrLP;
        Tv y;

        // soft knee hard clip
        if(x >  t){
            const Tv d = x - t;
            y = t + d / (Tv(1) + d * d);
        }
        else if(x < -t){
            const Tv d = x + t;
            y = -t + d / (Tv(1) + d * d);
        }
        else{
            y = x;
        }

        // asymmetry (even harmonics)
        y += asymLP * y * y;

        return y;
    }

    inline Tv loudComp() const {
        return Tv(1) / (Tv(1) + Tv(0.3) * drvLP);
    }
};

} // namespace gam
