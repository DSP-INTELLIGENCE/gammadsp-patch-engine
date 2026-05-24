#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class RBJFilter : public Td {
public:
    enum Type {
        LPF, HPF, BPF_SKIRT_Q, BPF_PEAK_0DB,
        NOTCH, ALLPASS, PEAKING, LOW_SHELF, HIGH_SHELF, THRU
    };

    RBJFilter(Type type = LPF,
              T freqHz = T(1000),
              T q = T(0.707),
              T gainDb = T(0),
              T shelfSlope = T(1))
    : mType(type)
    {
        set(freqHz, q, gainDb, shelfSlope);
        reset();
    }

    void reset(T v = T(0)){
        s1 = s2 = v;
    }

    void setType(Type t){
        mType = t;
        compute();
    }

    void freq(T f){
        mFreq = clampFreq(f);
        compute();
    }

    void res(T q){
        mQ = std::max(q, T(1e-6));
        compute();
    }

    void gain(T db){
        mGainDb = db;
        compute();
    }

    void slope(T s){
        mSlope = std::max(s, T(1e-6));
        compute();
    }

    void set(T f, T q, T g, T s){
        mFreq   = clampFreq(f);
        mQ      = std::max(q, T(1e-6));
        mGainDb = g;
        mSlope  = std::max(s, T(1e-6));
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
    Type mType;
    T mFreq   = T(1000);
    T mQ      = T(0.707);
    T mGainDb = T(0);
    T mSlope  = T(1);

    // coeffs
    T b0 = T(1), b1 = T(0), b2 = T(0);
    T a1 = T(0), a2 = T(0);

    // DF-II state
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

        const T A = std::pow(T(10), mGainDb / T(40));
        T alpha = T(0);

        T bb0 = T(1), bb1 = T(0), bb2 = T(0);
        T aa0 = T(1), aa1 = T(0), aa2 = T(0);

        switch(mType){
        case LPF:
            alpha = sw / (T(2) * mQ);
            bb0 = (T(1) - cw) * T(0.5);
            bb1 =  T(1) - cw;
            bb2 = bb0;
            aa0 =  T(1) + alpha;
            aa1 = -T(2) * cw;
            aa2 =  T(1) - alpha;
            break;

        case HPF:
            alpha = sw / (T(2) * mQ);
            bb0 = (T(1) + cw) * T(0.5);
            bb1 = -(T(1) + cw);
            bb2 = bb0;
            aa0 =  T(1) + alpha;
            aa1 = -T(2) * cw;
            aa2 =  T(1) - alpha;
            break;

        case BPF_SKIRT_Q:
            alpha = sw / (T(2) * mQ);
            bb0 =  sw * T(0.5);
            bb1 =  T(0);
            bb2 = -bb0;
            aa0 =  T(1) + alpha;
            aa1 = -T(2) * cw;
            aa2 =  T(1) - alpha;
            break;

        case BPF_PEAK_0DB:
            alpha = sw / (T(2) * mQ);
            bb0 =  alpha;
            bb1 =  T(0);
            bb2 = -alpha;
            aa0 =  T(1) + alpha;
            aa1 = -T(2) * cw;
            aa2 =  T(1) - alpha;
            break;

        case NOTCH:
            alpha = sw / (T(2) * mQ);
            bb0 =  T(1);
            bb1 = -T(2) * cw;
            bb2 =  T(1);
            aa0 =  T(1) + alpha;
            aa1 = bb1;
            aa2 =  T(1) - alpha;
            break;

        case ALLPASS:
            alpha = sw / (T(2) * mQ);
            bb0 =  T(1) - alpha;
            bb1 = -T(2) * cw;
            bb2 =  T(1) + alpha;
            aa0 =  T(1) + alpha;
            aa1 = bb1;
            aa2 =  T(1) - alpha;
            break;

        case PEAKING:
            alpha = sw / (T(2) * mQ);
            bb0 =  T(1) + alpha * A;
            bb1 = -T(2) * cw;
            bb2 =  T(1) - alpha * A;
            aa0 =  T(1) + alpha / A;
            aa1 = bb1;
            aa2 =  T(1) - alpha / A;
            break;

        case LOW_SHELF:
        {
            const T term = (A + T(1) / A) * (T(1) / mSlope - T(1)) + T(2);
            alpha = (sw * T(0.5)) * std::sqrt(std::max(T(0), term));
            const T twoSqrtAalpha = T(2) * std::sqrt(A) * alpha;

            bb0 =  A * ((A + T(1)) - (A - T(1)) * cw + twoSqrtAalpha);
            bb1 =  T(2) * A * ((A - T(1)) - (A + T(1)) * cw);
            bb2 =  A * ((A + T(1)) - (A - T(1)) * cw - twoSqrtAalpha);
            aa0 =  (A + T(1)) + (A - T(1)) * cw + twoSqrtAalpha;
            aa1 =  -T(2) * ((A - T(1)) + (A + T(1)) * cw);
            aa2 =  (A + T(1)) + (A - T(1)) * cw - twoSqrtAalpha;
            break;
        }

        case HIGH_SHELF:
        {
            const T term = (A + T(1) / A) * (T(1) / mSlope - T(1)) + T(2);
            alpha = (sw * T(0.5)) * std::sqrt(std::max(T(0), term));
            const T twoSqrtAalpha = T(2) * std::sqrt(A) * alpha;

            bb0 =    A * ((A + T(1)) + (A - T(1)) * cw + twoSqrtAalpha);
            bb1 =    -T(2) * A * ((A - T(1)) + (A + T(1)) * cw);
            bb2 =    A * ((A + T(1)) + (A - T(1)) * cw - twoSqrtAalpha);
            aa0 =    (A + T(1)) - (A - T(1)) * cw + twoSqrtAalpha;
            aa1 =    T(2) * ((A - T(1)) - (A + T(1)) * cw);
            aa2 =    (A + T(1)) - (A - T(1)) * cw - twoSqrtAalpha;
            break;
        }

        case THRU:
        default:
            bb0 = T(1); bb1 = bb2 = T(0);
            aa0 = T(1); aa1 = aa2 = T(0);
            break;
        }

        const T invA0 = T(1) / aa0;
        b0 = bb0 * invA0;
        b1 = bb1 * invA0;
        b2 = bb2 * invA0;
        a1 = aa1 * invA0;
        a2 = aa2 * invA0;
    }
};

} // namespace gam
