#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "AdaptiveThreshold.hpp"
#include "AdaptiveControllerAR.hpp"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveGate : public Td {
public:
    AdaptiveGate(
        T meanTime    = T(0.5),
        T devTime     = T(0.05),
        T sensitivity = T(1.5)
    )
    : thresh(meanTime, devTime, sensitivity)
    {
        attackTime(T(0.005));
        releaseTime(T(0.050));
        holdTime(T(0.02));
        hysteresis(T(0.5));
        reset();
    }

    // ---------------- lifecycle ----------------

    void reset()
    {
        gateOpen = false;
        holdCounter = 0;
        gainCtrl.reset(T(0));
    }

    // ---------------- parameters ----------------

    void attackTime(T sec)  { gainCtrl.attackTime(sec); }
    void releaseTime(T sec) { gainCtrl.releaseTime(sec); }

    /// Hold time after closing condition (seconds)
    void holdTime(T sec)
    {
        mHoldTime = scl::max(sec, T(0));
        updateHoldSamples();
    }

    /// Hysteresis factor (0..1)
    /// 0 = no hysteresis, 1 = large gap
    void hysteresis(T h)
    {
        mHys = scl::clip(h, T(0), T(1));
    }

    /// Threshold sensitivity
    void sensitivity(T s)
    {
        thresh.sensitivity(s);
    }

    /// Direct access if needed
    AdaptiveThreshold<T, Td>& threshold() { return thresh; }

    // ---------------- processing ----------------

    /// Input: energy / envelope (not raw audio)
    /// Output: linear gain (0..1)
    T operator()(T x)
    {
        // Update adaptive threshold
        T Tbase = thresh(x);

        // Hysteresis thresholds
        T Thigh = Tbase * (T(1) + mHys);
        T Tlow  = Tbase * (T(1) - mHys);

        // Gate logic
        if (gateOpen)
        {
            if (x < Tlow)
            {
                if (holdCounter == 0)
                    gateOpen = false;
                else
                    --holdCounter;
            }
            else
            {
                holdCounter = mHoldSamples;
            }
        }
        else
        {
            if (x > Thigh)
            {
                gateOpen = true;
                holdCounter = mHoldSamples;
            }
        }

        // Smooth gain (0 or 1)
        gainCtrl.target(gateOpen ? T(1) : T(0));
        return gainCtrl();
    }

    // ---------------- diagnostics ----------------

    bool isOpen() const { return gateOpen; }
    T thresholdValue() const { return thresh.value(); }

    // ---------------- domain ----------------

    void onDomainChange(double)
    {
        thresh.onDomainChange(0);
        updateHoldSamples();
    }

private:
    void updateHoldSamples()
    {
        auto fs = Td::spu();
        mHoldSamples = (fs > 0)
            ? static_cast<unsigned>(mHoldTime * fs)
            : 0;
    }

    // ---------------- state ----------------

    AdaptiveThreshold<T, Td>     thresh;
    AdaptiveControllerAR<T, Td>  gainCtrl;

    bool gateOpen = false;

    T mHys = T(0.5);

    T mHoldTime = T(0.02);
    unsigned mHoldSamples = 0;
    unsigned holdCounter  = 0;
};

} // namespace gam
