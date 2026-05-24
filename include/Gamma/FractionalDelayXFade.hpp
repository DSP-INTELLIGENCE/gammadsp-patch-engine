#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include <algorithm>

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Cubic,
    class Td = GAM_DEFAULT_DOMAIN
>
class FractionalDelayXFade {
public:
    using DelayT = Delay<Tv, Si, Td>;

    /// maxDelaySamples: buffer size in samples
    explicit FractionalDelayXFade(float maxDelaySamples)
    : mDelay(maxDelaySamples * Td::ups(), 0.0f)
    {}

    /// Set crossfade time in samples
    void fadeTimeSamples(float samples){
        mFadeInc = (samples > 0.f) ? 1.f / samples : 1.f;
    }

    /// Set delay (fractional samples, artifact-free)
    void delaySamplesR(float samples){
        samples = clampDelay(samples);

        if (!mFading){
            mDelayB = samples;
            mFade   = 0.f;
            mFading = true;
        }
        else{
            // already fading → retarget B
            mDelayB = samples;
        }
    }

    /// Process input sample
    Tv operator()(const Tv& x){
        // read both taps
        mDelay.delaySamplesR(mDelayA);
        Tv a = mDelay.read();

        mDelay.delaySamplesR(mDelayB);
        Tv b = mDelay.read();

        // crossfade
        Tv y = a * (1.f - mFade) + b * mFade;

        // write once
        mDelay.write(x);

        // advance fade
        if (mFading){
            mFade += mFadeInc;
            if (mFade >= 1.f){
                mFade     = 0.f;
                mDelayA   = mDelayB;
                mFading   = false;
            }
        }

        return y;
    }

    /// Read-only tap
    Tv operator()() const {
        return mDelay.read();
    }

private:
    DelayT mDelay;

    float mDelayA = 0.f;   // active delay
    float mDelayB = 0.f;   // target delay

    float mFade   = 0.f;
    float mFadeInc = 0.001f;
    bool  mFading = false;

    float clampDelay(float s) const {
        float maxS = mDelay.maxDelay() * Td::spu();
        return std::clamp(s, 0.f, maxS - 1.f);
    }
};

} // namespace gam
