#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class RBJNotch : public Td {
public:
    RBJNotch(T freqHz = T(1000), T Q = T(0.707)){
        set(freqHz, Q);
        reset();
    }

    void reset(T v = T(0)){
        s1 = s2 = v;
    }

    /// Center frequency (Hz)
    void freq(T f){
        mFreq = clampFreq(f);
        compute();
    }

    /// Quality factor (Q)
    void q(T q){
        mQ = std::max(q, T(1e-6));
        compute();
    }

    void set(T f, T q){
        mFreq = clampFreq(f);
        mQ    = std::max(q, T(1e-6));
        compute();
    }

    /// DF-II Transposed
    inline T operator()(T x){
        const T y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void onDomainChange(double){
        compute();
    }

private:
    // parameters
    T mFreq = T(1000);
    T mQ    = T(0.707);

    // coefficients
    T b0 = T(0), b1 = T(0), b2 = T(0);
    T a1 = T(0), a2 = T(0);

    // DF-II transposed state
    T s1 = T(0), s2 = T(0);

    inline T clampFreq(T f) const {
        const T fs = T(Td::spu());
        return std::clamp(f, T(1e-4), fs * T(0.499));
    }

    void compute(){
        const T fs = T(Td::spu());
        if(fs <= T(0)) return;

        const T w0 = T(2) * T(M_PI) * mFreq / fs;
        const T cw = std::cos(w0);
        const T sw = std::sin(w0);

        const T alpha = sw / (T(2) * mQ);

        // RBJ Notch
        const T bb0 =  T(1);
        const T bb1 = -T(2) * cw;
        const T bb2 =  T(1);
        const T aa0 =  T(1) + alpha;
        const T aa1 = bb1;
        const T aa2 =  T(1) - alpha;

        const T invA0 = T(1) / aa0;

        b0 = bb0 * invA0;
        b1 = bb1 * invA0;
        b2 = bb2 * invA0;
        a1 = aa1 * invA0;
        a2 = aa2 * invA0;
    }
};

} // namespace gam
