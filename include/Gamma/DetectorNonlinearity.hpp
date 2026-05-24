#pragma once
#include <cmath>
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DetectorNonlinearity : public Td {
public:
    enum class Mode {
        Linear,       // env = x
        Power,        // env = x^p
        Logarithmic,  // env = log(1 + kx)
        Saturating,   // env = tanh(kx)
        Asymmetric    // curvature asymmetry
    };

    DetectorNonlinearity() = default;

    void reset(){}

    void mode(Mode m){ mMode = m; }

    // Controls
    void drive(T v){ mDrive = scl::max(v, T(0)); }
    void power(T v){ mPower = scl::max(v, T(0.1)); }
    void asymmetry(T v){
        mAsym = scl::clip(v, T(1), T(-1));
    }

    void normalize(bool v){ mNormalize = v; }

    /// Input must be rectified (>= 0)
    inline T operator()(T x){
        x = scl::max(x, T(0));
        T y = x;

        switch(mMode){
            case Mode::Linear:
                y = x;
                break;

            case Mode::Power:
                y = std::pow(x, mPower);
                break;

            case Mode::Logarithmic:
                // perceptual detector, stable near zero
                y = std::log1p(mDrive * x);
                break;

            case Mode::Saturating:
                y = std::tanh(mDrive * x);
                break;

            case Mode::Asymmetric:{
                // curvature blend (not temporal!)
                T up   = std::tanh(mDrive * x);
                T down = std::pow(x, mPower);

                // asym ∈ [-1,1] → mix ∈ [0,1]
                T mix = T(0.5) * (mAsym + T(1));
                y = (T(1) - mix) * down + mix * up;
                break;
            }
        }

        // Optional output normalization
        if(mNormalize && mMode == Mode::Logarithmic){
            y /= std::log1p(mDrive);
        }

        return y;
    }

private:
    Mode mMode { Mode::Linear };

    T mDrive { T(1) };
    T mPower { T(1) };
    T mAsym  { T(0) };

    bool mNormalize { false };
};

} // namespace gam
