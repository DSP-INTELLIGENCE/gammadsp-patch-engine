#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "AdaptiveThreshold.hpp"
#include "AdaptiveControllerAR.hpp"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveExpander : public Td {
public:
    enum class Mode {
        Downward,   // noise reduction / cleanup
        Upward      // detail enhancement
    };

    AdaptiveExpander(
        T meanTime    = T(0.5),
        T devTime     = T(0.05),
        T sensitivity = T(1.5)
    )
    : thresh(meanTime, devTime, sensitivity)
    {
        mode(Mode::Downward);
        ratio(T(2));               // gentle by default
        attackTime(T(0.005));
        releaseTime(T(0.080));
        reset();
    }

    // ---------------- lifecycle ----------------

    void reset()
    {
        gainCtrl.reset(T(1));
    }

    // ---------------- parameters ----------------

    void mode(Mode m)      { mMode = m; }
    void ratio(T r)        { mRatio = scl::max(r, T(1)); }

    void attackTime(T sec)  { gainCtrl.attackTime(sec); }
    void releaseTime(T sec) { gainCtrl.releaseTime(sec); }

    void sensitivity(T s)   { thresh.sensitivity(s); }

    AdaptiveThreshold<T, Td>& threshold() { return thresh; }

    // ---------------- processing ----------------

    /// Input: energy / envelope (>=0)
    /// Output: linear gain
    T operator()(T x)
    {
        T Tbase = thresh(x);
        T eps   = T(1e-12);

        // Normalized level
        T r = x / (Tbase + eps);

        T targetGain = T(1);

        if (mMode == Mode::Downward)
        {
            if (r < T(1))
                targetGain = scl::pow(r, mRatio - T(1));
        }
        else // Upward expansion
        {
            if (r > T(1))
                targetGain = scl::pow(r, mRatio - T(1));
        }

        // Smooth gain
        gainCtrl.target(targetGain);
        return gainCtrl();
    }

    // ---------------- diagnostics ----------------

    T thresholdValue() const { return thresh.value(); }

    // ---------------- domain ----------------

    void onDomainChange(double)
    {
        thresh.onDomainChange(0);
    }

private:
    AdaptiveThreshold<T, Td>    thresh;
    AdaptiveControllerAR<T, Td> gainCtrl;

    Mode mMode = Mode::Downward;
    T    mRatio = T(2);
};

} // namespace gam
