#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"
#include <algorithm>
#include <cmath>

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class AllpassDelayXFade : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    AllpassDelayXFade() : mG(Tp(0)) {}

    // maxDelaySeconds: maximum delay in domain units (e.g., seconds)
    explicit AllpassDelayXFade(float maxDelaySeconds, float initDelaySeconds = 0.f, Tp g = Tp(0))
    : Base(maxDelaySeconds, initDelaySeconds), mG(Tp(0)) {
        this->g(g);
        // Initialize both heads to current delay
        float initSamp = initDelaySeconds * Td::spu();
        mDelayA = mDelayB = clampDelaySamples(initSamp);
    }

    /// Reset state (safe)
    void reset(){
        if(this->isSoleOwner()) this->zero();
        mFade = 0.f;
        mFading = false;
    }

    /// Set feedback coefficient (|g| < 1)
    Tp g() const { return mG; }
    void g(Tp v){ mG = scl::clip(v, Tp(-0.999), Tp(0.999)); }

    /// Crossfade time in samples (e.g., 64..1024 depending on taste)
    void fadeTimeSamples(float samples){
        mFadeInc = (samples > 0.f) ? (1.f / samples) : 1.f;
    }

    /// Set delay (domain units, e.g. seconds) — artifact-free
    void delay(float seconds){
        delaySamplesR(seconds * Td::spu());
    }

    /// Set delay (fractional samples) — artifact-free
    void delaySamplesR(float samples){
        samples = clampDelaySamples(samples);

        if(!mFading){
            // Start a new crossfade A -> B
            mDelayB = samples;
            mFade   = 0.f;
            mFading = true;
        }else{
            // Retarget B while fading (keeps output continuous)
            mDelayB = samples;
        }
    }

    /// Process Schroeder all-pass with dual-read crossfade
    Tv operator()(const Tv& x){
        // Read w[n - mA] and w[n - mB]
        const Tv wdA = Base::read(mDelayA * Td::ups());
        const Tv wdB = Base::read(mDelayB * Td::ups());

        // Crossfade (equal-power to avoid dips/bumps)
        const float f = mFading ? std::clamp(mFade, 0.f, 1.f) : 0.f;
        const float a = std::cos(f * float(M_PI_2));
        const float b = std::sin(f * float(M_PI_2));
        const Tv wd   = mFading ? (wdA * a + wdB * b) : wdA;

        // Schroeder all-pass core:
        // w[n] = x[n] + g*w[n-m]
        // y[n] = w[n-m] - g*w[n]
        const Tv w = x + wd * mG;
        const Tv y = wd - w * mG;

        // Single write (store w-state)
        Base::write(w);

        // Advance crossfade
        if(mFading){
            mFade += mFadeInc;
            if(mFade >= 1.f){
                mFade   = 0.f;
                mDelayA = mDelayB;
                mFading = false;
            }
        }

        return y;
    }

private:
    Tp mG;

    float mDelayA = 0.f;    // active delay (samples)
    float mDelayB = 0.f;    // target delay (samples)

    float mFade   = 0.f;    // 0..1
    float mFadeInc = 1.f / 128.f;
    bool  mFading = false;

    float clampDelaySamples(float s) const {
        // Base::maxDelay() is in domain units; convert to samples
        const float maxS = Base::maxDelay() * Td::spu();
        // Avoid exact max delay (can wrap in Gamma's fixed-point mapping)
        return std::clamp(s, 0.f, maxS - 1.f);
    }
};

} // namespace gam
