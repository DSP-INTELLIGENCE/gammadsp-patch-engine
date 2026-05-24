#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Containers.h"
#include "Gamma/Types.h"
#include "Gamma/rnd.h"
#include <cmath>

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class MultitapCombFlanger : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    struct Tap {
        float baseDelaySamples = 0.5f;   // very short
        float modDepthSamples  = 0.5f;
        float lfoPhase         = 0.f;
        float lfoInc           = 0.f;
        Tp    gain             = Tp(1);
        Tp    fbk              = Tp(0.7);
        bool  active           = true;

        float delaySmoothed    = 0.f;
        float slew             = 0.01f;  // faster than chorus
    };

    MultitapCombFlanger() = default;

    MultitapCombFlanger(float maxDelaySeconds, unsigned numTaps){
        Base::maxDelay(maxDelaySeconds, false);
        taps(numTaps);
        normalizeGains();
    }

    // ------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------

    void taps(unsigned n){
        mTaps.resize(n);
        for(auto& t : mTaps){
            t.delaySmoothed = t.baseDelaySamples;
        }
        normalizeGains();
    }

    unsigned taps() const { return mTaps.size(); }

    void rate(float hz){
        float inc = float(2.0 * M_PI * hz / Td::spu());
        for(auto& t : mTaps){
            t.lfoInc = inc;
        }
    }

    void depthMs(float ms){
        float samples = ms * 0.001f * Td::spu();
        for(auto& t : mTaps){
            t.modDepthSamples = samples;
        }
    }

    void baseDelayMs(float ms){
        float samples = ms * 0.001f * Td::spu();
        for(auto& t : mTaps){
            t.baseDelaySamples = samples;
        }
    }

    void feedback(Tp fbk){
        if(fbk < Tp(0)) fbk = Tp(0);
        if(fbk > Tp(0.99)) fbk = Tp(0.99);
        mGlobalFbk = fbk;
    }

    void mix(Tp wet){
        if(wet < Tp(0)) wet = Tp(0);
        if(wet > Tp(1)) wet = Tp(1);
        mWet = wet;
        mDry = Tp(1) - wet;
    }

    /// Small decorrelation between taps (init-time use)
    void randomize(float amount = 1.f){
        for(auto& t : mTaps){
            t.lfoPhase = rnd::uni(float(2.0 * M_PI));
            t.baseDelaySamples *= 1.f + rnd::tri(0.05f * amount);
            t.modDepthSamples  *= 1.f + rnd::tri(0.1f  * amount);
        }
    }

    // ------------------------------------------------------------
    // Audio
    // ------------------------------------------------------------

    Tv operator()(Tv x){
        Tv wet(0);
        Tv fbkSum(0);

        for(auto& t : mTaps){
            if(!t.active) continue;

            // --- LFO ---
            float lfo = std::sin(t.lfoPhase);
            t.lfoPhase += t.lfoInc;
            if(t.lfoPhase > float(2.0 * M_PI))
                t.lfoPhase -= float(2.0 * M_PI);

            float delayTarget =
                t.baseDelaySamples + lfo * t.modDepthSamples;

            // clamp near zero to avoid negative delay
            if(delayTarget < 0.f) delayTarget = 0.f;

            // --- smoothing ---
            t.delaySmoothed +=
                (delayTarget - t.delaySmoothed) * t.slew;

            float agoUnits = t.delaySmoothed * Td::ups();
            Tv d = Base::read(agoUnits);

            wet    += d * t.gain;
            fbkSum += d * t.fbk;
        }

        Base::write(x + fbkSum * mGlobalFbk);
        return x * mDry + wet * mWet;
    }

private:
    Array<Tap> mTaps;

    Tp mDry = Tp(1);
    Tp mWet = Tp(0.5);
    Tp mGlobalFbk = Tp(0.7);

    void normalizeGains(){
        if(mTaps.size() == 0) return;
        Tp g = Tp(1) / Tp(std::sqrt(double(mTaps.size())));
        for(auto& t : mTaps){
            t.gain = g;
        }
    }
};

} // namespace gam
