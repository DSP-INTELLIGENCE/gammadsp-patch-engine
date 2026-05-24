#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ============================================================
// Curve with exact 1st and 2nd antiderivatives (primitive-friendly)
// f(z)  = s * atan(k z)
// F(z)  = s * [ z*atan(k z) - (1/(2k)) ln(1 + k^2 z^2) ]
// G(z)  = s * [ 0.5 z^2 atan(k z)
//              - (z/(2k)) ln(1 + k^2 z^2)
//              + z/(2k)
//              - (1/(2k^2)) atan(k z) ]
//
// We use z = x +/- bias and mix with wp/wm for asymmetry.
// ============================================================
template<typename T>
struct TriodeCurveAtanMix {
    static_assert(std::is_floating_point_v<T>, "TriodeCurveAtanMix<T>: T must be float/double");

    struct Params {
        T k;     // curvature (>0)
        T bias;  // operating point offset (same units as x)
        T asym;  // [-1,1] weight between +bias and -bias branches
    };

    // scale to keep output in roughly [-1,1]
    static constexpr T scale() {
        // 2/pi
        return T(0.63661977236758134308);
    }

    static inline T f0(T z, T k){
        return scale() * std::atan(k * z);
    }

    static inline T F0(T z, T k){
        // z*atan(kz) - (1/(2k)) ln(1 + k^2 z^2)
        const T kz  = k * z;
        const T a   = std::atan(kz);
        const T ln1 = std::log1p(kz * kz); // ln(1+k^2 z^2), stable for small kz
        return scale() * ( z * a - (T(0.5)/k) * ln1 );
    }

    static inline T G0(T z, T k){
        // 0.5 z^2 atan(kz)
        // - (z/(2k)) ln(1 + k^2 z^2)
        // + z/(2k)
        // - (1/(2k^2)) atan(kz)
        const T kz  = k * z;
        const T a   = std::atan(kz);
        const T ln1 = std::log1p(kz * kz);
        const T z2  = z * z;

        return scale() * (
            T(0.5) * z2 * a
            - (z * (T(0.5)/k)) * ln1
            + z * (T(0.5)/k)
            - a * (T(0.5)/(k*k))
        );
    }

    // Mixed branch transfer/primitive/second primitive
    static inline T transfer(T x, const Params& p){
        const T wp = T(0.5) * (T(1) + p.asym);
        const T wm = T(0.5) * (T(1) - p.asym);
        const T k  = p.k;
        const T b  = p.bias;

        // z+ = x + b, z- = x - b
        return wp * f0(x + b, k) + wm * f0(x - b, k);
    }

    static inline T primitive(T x, const Params& p){
        const T wp = T(0.5) * (T(1) + p.asym);
        const T wm = T(0.5) * (T(1) - p.asym);
        const T k  = p.k;
        const T b  = p.bias;
        return wp * F0(x + b, k) + wm * F0(x - b, k);
    }

    static inline T primitive2(T x, const Params& p){
        const T wp = T(0.5) * (T(1) + p.asym);
        const T wm = T(0.5) * (T(1) - p.asym);
        const T k  = p.k;
        const T b  = p.bias;
        return wp * G0(x + b, k) + wm * G0(x - b, k);
    }
};

} // namespace gam
