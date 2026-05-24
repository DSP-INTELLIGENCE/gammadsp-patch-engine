#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "AdaptiveThreshold.hpp"
#include "AdaptiveGainComputer.hpp"
#include "AdaptiveTimeConstants.hpp"
#include "AdaptiveControllerAR.hpp"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class AdaptiveCompressor : public Td {
public:
    AdaptiveCompressor()
    {
        reset();
    }

    // ------------------------------------------------
    // Configuration (analysis layer)
    // ------------------------------------------------

    AdaptiveThreshold<T, Td>& threshold()    { return mThresh; }
    AdaptiveGainComputer<T, Td>& gainComp()  { return mGainComp; }
    AdaptiveTimeConstants<T, Td>& timing()   { return mTiming; }

    void setMakeupDb(T db) { mMakeup = db; }

    // ------------------------------------------------
    // Lifecycle
    // ------------------------------------------------

    void reset()
    {
        mGainSmooth.reset(T(0));
        mCurrentGainDb = T(0);
    }

    // ------------------------------------------------
    // Processing
    // ------------------------------------------------

    /// Input: detector level (linear, e.g. RMS or envelope)
    /// Output: linear gain
    T control(T env)
    {
        // --- Convert detector level to dB ---
        T envDb = T(20) * scl::log10(scl::max(env, T(1e-12)));

        // --- Adaptive threshold ---
        T threshDb = mThresh(envDb);

        // --- Gain computer (target gain in dB) ---
        mGainComp.setThresholdDb(threshDb);
        T targetGainDb = mGainComp(envDb);

        // --- Adaptive timing ---
        T atk, rel;
        mTiming(targetGainDb, mCurrentGainDb, atk, rel);
        mGainSmooth.attackTime(atk);
        mGainSmooth.releaseTime(rel);

        // --- Smooth gain ---
        mGainSmooth.target(targetGainDb);
        mCurrentGainDb = mGainSmooth();

        // --- Linear gain with makeup ---
        return scl::pow(T(10), (mCurrentGainDb + mMakeup) / T(20));
    }

    /// Audio-rate helper
    T operator()(T x, T env)
    {
        return x * control(env);
    }

    // ------------------------------------------------
    // Diagnostics
    // ------------------------------------------------

    T gainReductionDb() const { return -mCurrentGainDb; }

private:
    // --- analysis / intelligence ---
    AdaptiveThreshold<T, Td>     mThresh;
    AdaptiveGainComputer<T, Td>  mGainComp;
    AdaptiveTimeConstants<T, Td> mTiming;

    // --- smoothing ---
    AdaptiveControllerAR<T, Td>  mGainSmooth;

    // --- state ---
    T mCurrentGainDb { T(0) };
    T mMakeup        { T(0) };
};

} // namespace gam
