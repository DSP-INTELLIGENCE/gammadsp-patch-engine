#pragma once
#include <cmath> // sqrtf
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"
#include "Gamma/Filter.h" // OnePole

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Switchable,
    class Td = GAM_DEFAULT_DOMAIN
>
class CleanDelay : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    CleanDelay() { initDefaults(); }

    /// Allocate internal buffer
    explicit CleanDelay(float maxDelaySec)
    : Base(maxDelaySec) { initDefaults(); }

    /// Allocate + set initial delay
    CleanDelay(float maxDelaySec, float delaySec)
    : Base(maxDelaySec, delaySec) { initDefaults(); setDelay(delaySec); }

    // -------------------- Controls --------------------

    /// Set delay time in seconds (will be smoothed)
    void setDelay(float sec){
        // clamp to safe
        sec = scl::clip(sec, maxDelay(), kMinDelaySec);
        mDelayTarget = Tv(sec);
    }

    float delay() const { return (float)mDelaySmoothed; }
    float maxDelay() const { return Base::maxDelay(); }

    /// Feedback gain (0.. <1)
    void setFeedback(float fb){
        // keep strictly < 1 to guarantee stability
        fb = scl::clip(fb, 0.9995f, 0.f);
        mFeedback = Tv(fb);
    }

    float feedback() const { return (float)mFeedback; }

    /// Wet mix in [0..1] (equal-power by default)
    void setMix(float mix){
        mix = scl::clip(mix, 1.f, 0.f);
        mMix = Tv(mix);
        updateMixGains();
    }

    float mix() const { return (float)mMix; }

    /// Equal-power mixing (recommended for “clean” loudness constancy)
    void setEqualPower(bool v){
        mEqualPower = v;
        updateMixGains();
    }

    bool equalPower() const { return mEqualPower; }

    /// Optional: set interpolator type when Si = ipl::Switchable
    void setIpolType(ipl::Type t){ Base::ipolType(t); }

    /// Optional: freeze the buffer (infinite hold) by forcing write input to 0 and fb=1
    void freeze(bool v){ mFreeze = v; }

    bool frozen() const { return mFreeze; }

    // -------------------- Lifecycle --------------------

    void reset(){
        if(this->isSoleOwner()) this->zero();
        mDelaySmoothed = mDelayTarget;
        mDc.reset(Tv(0));
    }

    void clear(){
        if(this->isSoleOwner()) this->zero();
    }

    void onDomainChange(double /*r*/){
        // keep smoothing & DC blocker consistent
        setDelay((float)mDelayTarget);
        mDelaySmoothed = mDelayTarget;
        mDc.freq(mDcFreqHz);
        updateMixGains();
    }

    // -------------------- Audio --------------------

    /// Mono in -> mono out
    inline Tv operator()(Tv in){
        // Smooth delay time (critical for “clean” modulation)
        // One-pole in seconds domain: y += a*(x - y)
        mDelaySmoothed += mDelaySlew * (mDelayTarget - mDelaySmoothed);

        // Apply delay time
        Base::delay((float)mDelaySmoothed);

        // Read current delayed sample
        Tv d = Base::read();

        // DC-block feedback (prevents drift in long feedback loops)
        Tv fb = mDc(d) * mFeedback;

        // Write (once)
        if(mFreeze){
            // Hold current content: write only feedback (unity)
            Base::write(fb); // mFeedback should be near 1 externally if desired
        } else {
            Base::write(in + fb);
        }

        // Mix to output
        return in * mDry + d * mWet;
    }

private:
    static constexpr float kMinDelaySec = 0.0001f;

    // Parameters
    Tv   mDelayTarget   = Tv(0.25);
    Tv   mDelaySmoothed = Tv(0.25);
    Tv   mDelaySlew     = Tv(0.001); // smoothing coefficient per-sample (set in initDefaults)

    Tv   mFeedback = Tv(0);
    Tv   mMix      = Tv(0.5);
    Tv   mDry      = Tv(0.70710678); // updated by updateMixGains()
    Tv   mWet      = Tv(0.70710678);

    bool mEqualPower = true;
    bool mFreeze     = false;

    // DC blocker on feedback tap (very low cutoff)
    OnePole<Tv, Tv, Tv, Td> mDc;
    float mDcFreqHz = 5.f;

    void initDefaults(){
        // Default interpolation (if switchable): cubic is nicer under modulation, linear is cheaper.
        // Pick linear as “clean baseline”; caller can switch to CUBIC if modulating.
        if constexpr (std::is_same_v<Si<Tv>, ipl::Switchable<Tv>>){
            Base::ipolType(ipl::Type::LINEAR);
        }

        // Smoothing: pick ~10–20ms time constant for delay changes
        setDelaySmoothingMs(15.f);

        // DC blocker
        mDc.type(HIGH_PASS);
        mDc.freq(mDcFreqHz);

        setMix(0.5f);
        setFeedback(0.f);
        setDelay((float)mDelayTarget);
    }

    void setDelaySmoothingMs(float ms){
        // Convert time constant to per-sample smoothing coefficient:
        // a = 1 - exp(-1/(tau*fs))
        const float fs = (float)Td::spu();
        const float tau = scl::max(ms * 0.001f, 1e-5f);
        const float a = 1.f - std::exp(-1.f / (tau * fs));
        mDelaySlew = Tv(scl::clip(a, 1.f, 0.000001f));
    }

    void updateMixGains(){
        if(mEqualPower){
            // equal-power
            float mix = (float)mMix;
            float dry = std::sqrt(scl::max(1.f - mix, 0.f));
            float wet = std::sqrt(scl::max(mix, 0.f));
            mDry = Tv(dry);
            mWet = Tv(wet);
        } else {
            mDry = Tv(1) - mMix;
            mWet = mMix;
        }
    }
};

} // namespace gam
