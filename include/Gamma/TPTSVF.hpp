#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class TPTSVF : public Td {
public:
    TPTSVF(T fc = T(1000), T q = T(0.707)){
        reset();
        freq(fc);
        res(q);
    }

    void reset(T ic1 = T(0), T ic2 = T(0)){
        ic1eq = ic1;
        ic2eq = ic2;
        lp = bp = hp = notch = peak = T(0);
    }

    void freq(T fc){
        mFc = std::max(fc, T(0));
        recalc();
    }

    void res(T q){
        mQ = std::max(q, T(1e-6));
        recalc();
    }

    /// Process one sample (returns lowpass)
    inline T operator()(T x){
        process(x);
        return lp;
    }

    inline void process(T x){
        // Zavalishin TPT SVF
        // g = tan(pi*fc/fs), k = 1/Q
        // a1 = 1 / (1 + g*(g + k))
        // a2 = g*a1
        // a3 = g*a2
        //
        // v3 = x - ic2eq
        // v1 = a1*ic1eq + a2*v3
        // v2 = ic2eq + a2*ic1eq + a3*v3
        // ic1eq = 2*v1 - ic1eq
        // ic2eq = 2*v2 - ic2eq
        //
        // hp = x - k*v1 - v2
        // bp = v1
        // lp = v2
        // notch = hp + lp
        // peak  = lp - hp   (common “peak” variant)

        const T v3 = x - ic2eq;
        const T v1 = a1 * ic1eq + a2 * v3;
        const T v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = T(2) * v1 - ic1eq;
        ic2eq = T(2) * v2 - ic2eq;

        hp = x - k * v1 - v2;
        bp = v1;
        lp = v2;

        notch = hp + lp;
        peak  = lp - hp;
    }

    T low()      const { return lp; }
    T band()     const { return bp; }
    T high()     const { return hp; }
    T notchOut() const { return notch; }
    T peakOut()  const { return peak; }

    void onDomainChange(double){ recalc(); }

private:
    // Parameters
    T mFc = T(1000);
    T mQ  = T(0.707);

    // Coefficients
    T g  = T(0);
    T k  = T(0);
    T a1 = T(1);
    T a2 = T(0);
    T a3 = T(0);

    // States (equivalent integrator currents)
    T ic1eq = T(0);
    T ic2eq = T(0);

    // Outputs
    T lp = T(0), bp = T(0), hp = T(0);
    T notch = T(0), peak = T(0);

    void recalc(){
        const T fs = T(Td::spu());
        if(fs <= T(0)){
            g = T(0); k = T(1);
            a1 = T(1); a2 = T(0); a3 = T(0);
            return;
        }

        // Clamp cutoff safely (< Nyquist)
        const T nyq = fs * T(0.5);
        const T fc  = std::clamp(mFc, T(0), nyq * T(0.999));
        mFc = fc;

        // TPT coefficient
        const T pi = T(3.14159265358979323846);
        g = std::tan(pi * (fc / fs));

        k = T(1) / mQ;

        const T denom = T(1) + g * (g + k);
        a1 = T(1) / denom;
        a2 = g * a1;
        a3 = g * a2;
    }
};

} // namespace gam
