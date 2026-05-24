#pragma once
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodeGainStage : public Td {
public:
    AnalogGainCell<Tv> cell;
    Triode<Tv>         triode;
    TPT1Pole<Tv>       plateLP;

    /// DC plate supply (normalized units)
    Tv plateSupply = Tv(1.0);

    void reset(){
        cell.reset();
        plateLP.reset(plateSupply);
        mPlateV = plateSupply;
    }

    inline Tv operator()(Tv x){
        // --- grid drive ---
        const Tv vgk = cell(x);

        // --- plate voltage (must never hit zero) ---
        const Tv vpk = std::max(mPlateV, Tv(0.05));

        // --- tube conduction ---
        const Tv ip = triode(vgk, vpk);

        // --- plate load integration (RC behavior) ---
        mPlateV = plateLP(-ip + plateSupply);

        // output is inverted plate swing
        return -mPlateV;
    }

    void onDomainChange(double){
        // Plate RC pole ≈ output bandwidth
        plateLP.freq(Tv(8000));
        reset();
    }

private:
    Tv mPlateV { Tv(1) };
};

} // namespace gam
