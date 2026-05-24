#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ------------------------------------------------------------
// Curve: odd-symmetric Shockley diode-pair current
// f(x) = 2*Is*sinh(x/Vt)
// F1(x) = 2*Is*Vt*cosh(x/Vt)
// F2(x) = 2*Is*Vt*Vt*sinh(x/Vt)
// ------------------------------------------------------------
template<typename T>
struct ShockleySinh {
    static_assert(std::is_floating_point_v<T>, "ShockleySinh<T>: T must be float/double");

    struct Params {
        T Is; // saturation current
        T Vt; // thermal voltage (n*Vt if you want ideality folded in)
    };

    static inline T transfer(T x, const Params& p){
        return T(2) * p.Is * std::sinh(x / p.Vt);
    }

    static inline T primitive1(T x, const Params& p){
        return T(2) * p.Is * p.Vt * std::cosh(x / p.Vt);
    }

    static inline T primitive2(T x, const Params& p){
        return T(2) * p.Is * p.Vt * p.Vt * std::sinh(x / p.Vt);
    }
};


// ============================================================
// Generic ADAA1 wrapper for any curve with transfer + primitive1
// ============================================================
template<class T, class Curve, class Td = GAM_DEFAULT_DOMAIN>
class ADAA1 : public Td {
public:
    using Params = typename Curve::Params;

    ADAA1(){ reset(); }

    void reset(){ mHasPrev=false; mX1=T(0); }
    void onDomainChange(double){}

    inline T operator()(T x, const Params& p){
        if(!std::isfinite(x)) return T(0);

        if(!mHasPrev){
            mHasPrev = true;
            mX1 = x;
            return Curve::transfer(x, p);
        }

        const T x1 = mX1;
        mX1 = x;

        const T dx = x - x1;
        const T eps = scaledEps(x, x1);

        if(std::abs(dx) <= eps){
            return Curve::transfer(T(0.5)*(x + x1), p);
        }

        const T F0 = Curve::primitive1(x,  p);
        const T F1 = Curve::primitive1(x1, p);
        const T y  = (F0 - F1) / dx;
        return std::isfinite(y) ? y : T(0);
    }

private:
    static inline T scaledEps(T x0, T x1){
        const T C = T(16);
        return C * std::numeric_limits<T>::epsilon() * (T(1) + std::abs(x0) + std::abs(x1));
    }

    bool mHasPrev=false;
    T    mX1=T(0);
};


// ============================================================
// Generic ADAA2 wrapper for any curve with primitive2
// y = 2 * G[x0,x1,x2]  (2nd divided difference of primitive2)
// ============================================================
template<class T, class Curve, class Td = GAM_DEFAULT_DOMAIN>
class ADAA2 : public Td {
public:
    using Params = typename Curve::Params;

    ADAA2(){ reset(); }

    void reset(){ mCount=0; mX1=mX2=T(0); }
    void onDomainChange(double){}

    inline T operator()(T x, const Params& p){
        if(!std::isfinite(x)) return T(0);

        if(mCount == 0){
            mX1 = x; mCount = 1;
            return Curve::transfer(x, p);
        }
        if(mCount == 1){
            mX2 = mX1;
            mX1 = x;
            mCount = 2;
            // warmup via ADAA1-ish quotient
            return adaa1(x, mX2, p);
        }

        return adaa2(x, p);
    }

private:
    static inline T scaledEps3(T x0, T x1, T x2){
        const T C = T(32);
        return C * std::numeric_limits<T>::epsilon()
             * (T(1) + std::abs(x0) + std::abs(x1) + std::abs(x2));
    }

    static inline T adaa1(T x0, T x1, const Params& p){
        const T dx = x0 - x1;
        const T eps = T(16) * std::numeric_limits<T>::epsilon() * (T(1) + std::abs(x0) + std::abs(x1));
        if(std::abs(dx) <= eps){
            return Curve::transfer(T(0.5)*(x0 + x1), p);
        }
        const T F0 = Curve::primitive1(x0, p);
        const T F1 = Curve::primitive1(x1, p);
        const T y  = (F0 - F1) / dx;
        return std::isfinite(y) ? y : T(0);
    }

    inline T adaa2(T x0, const Params& p){
        const T x1 = mX1;
        const T x2 = mX2;

        // shift
        mX2 = mX1;
        mX1 = x0;

        const T d01 = x0 - x1;
        const T d12 = x1 - x2;
        const T d02 = x0 - x2;

        const T eps = scaledEps3(x0,x1,x2);
        if(std::abs(d01) <= eps || std::abs(d12) <= eps || std::abs(d02) <= eps){
            return adaa1(x0, x1, p);
        }

        const T G0  = Curve::primitive2(x0, p);
        const T G1  = Curve::primitive2(x1, p);
        const T G2  = Curve::primitive2(x2, p);

        const T G01 = (G0 - G1) / d01;
        const T G12 = (G1 - G2) / d12;

        const T y   = T(2) * (G01 - G12) / d02;
        return std::isfinite(y) ? y : T(0);
    }

    int mCount=0;
    T   mX1=T(0), mX2=T(0);
};


// ------------------------------------------------------------
// Ready-to-use: Shockley sinh clipper with ADAA1 / ADAA2
// (still "no smoothing"; parameters can jump per sample)
// ------------------------------------------------------------
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ShockleyClipperADAA1 : public Td {
public:
    ShockleyClipperADAA1(){}

    void reset(){ mADAA.reset(); }
    void onDomainChange(double){}

    void Is(T v){ mIs = std::max(v, T(1e-18)); }
    void Vt(T v){ mVt = std::max(v, T(1e-9));  }
    void drive(T v){ mDrive = std::max(v, T(0)); }
    void trim(T v){ mTrim = v; }

    inline T operator()(T x){
        typename ShockleySinh<T>::Params p{mIs, mVt};
        return mADAA(x * mDrive, p) * mTrim;
    }

private:
    ADAA1<T, ShockleySinh<T>, Td> mADAA;
    T mIs=T(1e-12), mVt=T(0.026), mDrive=T(1), mTrim=T(1);
};

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class ShockleyClipperADAA2 : public Td {
public:
    ShockleyClipperADAA2(){}

    void reset(){ mADAA.reset(); }
    void onDomainChange(double){}

    void Is(T v){ mIs = std::max(v, T(1e-18)); }
    void Vt(T v){ mVt = std::max(v, T(1e-9));  }
    void drive(T v){ mDrive = std::max(v, T(0)); }
    void trim(T v){ mTrim = v; }

    inline T operator()(T x){
        typename ShockleySinh<T>::Params p{mIs, mVt};
        return mADAA(x * mDrive, p) * mTrim;
    }

private:
    ADAA2<T, ShockleySinh<T>, Td> mADAA;
    T mIs=T(1e-12), mVt=T(0.026), mDrive=T(1), mTrim=T(1);
};

} // namespace gam
