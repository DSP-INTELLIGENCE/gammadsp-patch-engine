#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class EvenOddHarmonicMixer : public Td {
public:
    /// Controls [0..1]
    void even(Tv v){ mEven = std::clamp(v, Tv(0), Tv(1)); }
    void odd (Tv v){ mOdd  = std::clamp(v, Tv(0), Tv(1)); }

    Tv even() const { return mEven; }
    Tv odd()  const { return mOdd;  }

    void reset(){
        mEvenLP = mEven;
        mOddLP  = mOdd;
    }

    inline Tv operator()(Tv x){
        // smooth controls
        mEvenLP += mSmooth * (mEven - mEvenLP);
        mOddLP  += mSmooth * (mOdd  - mOddLP);

        // --- Even harmonics (asymmetry / bias) ---
        const Tv evenDrive = mEvenLP * Tv(2);
        Tv xp = x * (1 + mEvenLP);
        Tv xm = x * (1 - mEvenLP);
        Tv even = (x >= 0 ? sat(xp) : sat(xm)) - x;

        // --- Odd harmonics (symmetric drive) ---
        const Tv oddDrive = Tv(1) + mOddLP * Tv(3);
        Tv odd = sat(x * oddDrive) - x;

        // Energy-normalized blend
        const Tv norm = Tv(1) / (Tv(1) + mEvenLP + mOddLP);

        return x + norm * (mEvenLP * even + mOddLP * odd);
    }

    void onDomainChange(double){
        const Tv fs = Tv(Td::spu());
        if(fs <= Tv(0)) return;

        // smoothing time (seconds)
        const Tv smoothTime = Tv(0.03);
        mSmooth = Tv(1) - std::exp(-Tv(1) / (smoothTime * fs));
    }

private:
    static inline Tv sat(Tv x){
        return std::tanh(x);   // unity slope, clean harmonics
    }

private:
    // parameters
    Tv mEven { Tv(0.5) };
    Tv mOdd  { Tv(0.5) };

    // smoothed controls
    Tv mEvenLP { Tv(0.5) };
    Tv mOddLP  { Tv(0.5) };

    // smoothing coefficient
    Tv mSmooth { Tv(0.001) };
};

} // namespace gam
