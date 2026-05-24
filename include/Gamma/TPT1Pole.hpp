#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TPT1Pole : public Td {
public:
    TPT1Pole(T fc = T(1000)){
        reset();
        freq(fc);
    }

    void reset(T v = T(0)){
        s = v;
    }

    /// Set cutoff frequency (Hz)
    void freq(T fc){
        const T fs = T(Td::spu());

        // Clamp first
        fc = scl::clip(fc, T(1e-4), T(0.499) * fs);
        mFreq = fc;

        constexpr T pi = T(3.14159265358979323846);
        g = std::tan(pi * fc / fs);
        a = g / (T(1) + g);
    }

    /// Low-pass output
    inline T lp(T x){
        const T v = (x - s) * a;
        const T y = v + s;
        s = y + v;
        return y;
    }

    /// High-pass output
    inline T hp(T x){
        return x - lp(x);
    }

    /// All-pass output (ZDF, 1-pole)
    inline T ap(T x){
        const T y = lp(x);
        return x - T(2) * y;
    }

    /// Default operator = low-pass
    inline T operator()(T x){
        return lp(x);
    }

    void onDomainChange(double){
        freq(mFreq);
    }


    T g = T(0);
    T a = T(0);
    T s = T(0);
    T mFreq = T(1000);
};

} // namespace gam
