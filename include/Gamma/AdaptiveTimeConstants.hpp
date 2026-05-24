#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveTimeConstants : public Td {
public:
    void setBaseAttack(Tv sec)  { baseAtk = std::max(sec, Tv(1e-4)); }
    void setBaseRelease(Tv sec) { baseRel = std::max(sec, Tv(1e-4)); }

    void setAdaptAmount(Tv a)   { adapt  = std::clamp(a, Tv(0), Tv(1)); }
    void setMemory(Tv m)        { memory = std::clamp(m, Tv(0), Tv(0.999)); }

    void reset(){
        avgError = Tv(0);
        compSamples = 0;
    }

    /// targetDb < currentDb → compression active
    void compute(Tv targetDb, Tv currentDb,
                 Tv& outAtk, Tv& outRel)
    {
        const Tv error = std::abs(targetDb - currentDb);

        // Smoothed detector error
        avgError = (Tv(1) - memory) * error + memory * avgError;

        // Track continuous compression time (samples)
        if(targetDb < currentDb)
            ++compSamples;
        else
            compSamples = 0;

        // Normalize error (~12 dB reference)
        const Tv eNorm = std::min(Tv(1), avgError / Tv(12));

        // Faster attack for larger error
        const Tv atkScale = Tv(1) - adapt * eNorm;

        // Long compression → slower release
        const Tv longCompNorm =
            std::min(Tv(1), compSamples / compWindowSamples);

        outAtk = std::max(minAtk, baseAtk * atkScale);
        outRel = std::min(maxRel, baseRel * (Tv(1) + adapt * longCompNorm));
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        compWindowSamples = std::max(Tv(1), Tv(0.5) * fs); // 500 ms window
    }

private:
    // base times (seconds)
    Tv baseAtk { Tv(0.01) };
    Tv baseRel { Tv(0.1) };

    // bounds
    Tv minAtk { Tv(0.0005) };
    Tv maxRel { Tv(2.0) };

    // adaptation
    Tv adapt  { Tv(0.5) };
    Tv memory { Tv(0.9) };

    // state
    Tv avgError { Tv(0) };
    Tv compSamples { Tv(0) };
    Tv compWindowSamples { Tv(22050) }; // updated onDomainChange
};

} // namespace gam
