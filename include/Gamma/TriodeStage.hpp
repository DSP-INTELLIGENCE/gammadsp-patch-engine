#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

#include "Gamma/Triode.hpp"
namespace gam {

// -----------------------------------------
// Minimal TPT 1-pole with explicit state.
// LP is the state; HP is input - LP.
// -----------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TPTTriodePole : public Td {
public:
    TPTTriodePole(Tv fc = Tv(1000)){ freq(fc); }

    void reset(Tv v = Tv(0)){ s = v; }

    void freq(Tv fc){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        fc = std::clamp(fc, Tv(1e-4), Tv(0.49) * fs);
        mFc = fc;

        constexpr Tv pi = Tv(3.14159265358979323846);
        const Tv g = std::tan(pi * (fc / fs));
        a = g / (Tv(1) + g);
    }

    inline Tv lp(Tv x){
        // Trapezoidal integrator one-pole
        const Tv v = (x - s) * a;
        const Tv y = v + s;
        s = y + v;
        return y;
    }

    inline Tv hp(Tv x){
        const Tv ylp = lp(x);
        return x - ylp;
    }

    void onDomainChange(double){ freq(mFc); }

    Tv s { Tv(0) };    // exposed state (LP output)
private:
    Tv a  { Tv(0) };
    Tv mFc{ Tv(1000) };
};


// -----------------------------------------
// TriodeStage: coupling HP -> bias drift -> grid conduction
// -> triode current -> plate RC state (LP) -> output
// -----------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TriodeStage : public Td {
public:
    // --- Core device ---
    Triode<Tv,Td> triode;

    // --- Conditioning blocks ---
    TPTTriodePole<Tv,Td> inputHP;   // coupling capacitor HP
    TPTTriodePole<Tv,Td> biasLP;    // slow bias memory LP
    TPTTriodePole<Tv,Td> plateLP;   // plate RC LP (stores plate node)

    // --- User controls ---
    Tv drive      { Tv(2.5) }; // input gain
    Tv biasAmount { Tv(0.4) }; // envelope -> bias depth

    // --- “Circuit” scalars (normalized) ---
    Tv plateSupply { Tv(1) };  // normalized B+
    Tv plateMin    { Tv(0.05) };

    void reset(){
        inputHP.reset(Tv(0));
        biasLP.reset(Tv(0));
        plateLP.reset(plateSupply);

        mBias = Tv(0);
    }

    void onDomainChange(double){
        inputHP.freq(Tv(20));      // coupling cap corner
        biasLP.freq(Tv(5));        // bias drift speed (blocking distortion)
        plateLP.freq(Tv(8000));    // plate RC bandwidth
        // NOTE: don't auto-reset here; Gamma may call this mid-run.
    }

    inline Tv operator()(Tv x){
        // 1) input coupling (remove DC)
        x = inputHP.hp(x);

        // 2) envelope -> slow negative bias shift
        const Tv env = std::abs(x);
        const Tv targetBias = -biasAmount * env;
        mBias = biasLP.lp(targetBias);

        // 3) grid voltage
        Tv vgk = x * drive + mBias;

        // 4) grid conduction (soft positive clamp)
        // original: vgk -= 0.5 * max(0, vgk)
        if(vgk > Tv(0)) vgk *= Tv(0.5);

        // 5) plate voltage from previous state
        const Tv vpk = std::max(plateLP.s, plateMin);

        // 6) triode current (always >= 0)
        Tv ip = triode(vgk, vpk);

        // 7) plate node integration (RC load):
        // plateV tends toward plateSupply, pulled down by current.
        // This is a simple normalized mapping: plateDrive = plateSupply - ip.
        Tv plateDrive = plateSupply - ip;

        Tv plateV = plateLP.lp(plateDrive);
        if(plateV < plateMin){
            plateV = plateMin;
            plateLP.s = plateV; // keep state consistent
        }

        // 8) output: inverted plate swing around supply
        // (center it so 0 input gives ~0 output)
        return -(plateV - plateSupply);
    }

private:
    Tv mBias { Tv(0) };
};

} // namespace gam
