#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Containers.h"
#include "Gamma/Types.h"
#include <cmath>

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class MultitapCombChorus : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    struct Tap {
        // --- parameters ---
        float baseDelaySamples = 10.f;
        float modDepthSamples  = 2.f;
        float lfoPhase         = 0.f;
        float lfoInc           = 0.f;   // radians per sample
        Tp    gain             = Tp(1);
        Tp    fbk              = Tp(0);
        bool  active           = true;

        // --- smoothing ---
        float delaySmoothed    = 0.f;
        float slew             = 0.002f; // 0..1
    };

    MultitapCombChorus() = default;

    MultitapCombChorus(float maxDelaySeconds, unsigned numTaps){
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
        // radians per sample
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

    void mix(Tp wet){
        if(wet < Tp(0)) wet = Tp(0);
        if(wet > Tp(1)) wet = Tp(1);
        mWet = wet;
        mDry = Tp(1) - wet;
    }

    void feedback(Tp fbk){
        if(fbk < Tp(0)) fbk = Tp(0);
        if(fbk > Tp(0.95)) fbk = Tp(0.95);
        mGlobalFbk = fbk;
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

            // --- smoothing ---
            t.delaySmoothed +=
                (delayTarget - t.delaySmoothed) * t.slew;

            // convert samples -> domain units
            float agoUnits = t.delaySmoothed * Td::ups();

            Tv d = Base::read(agoUnits);

            wet    += d * t.gain;
            fbkSum += d * t.fbk;
        }

        Base::write(x + fbkSum * mGlobalFbk);

        return x * mDry + wet * mWet;
    }

    void randomize(float amount = 1.f){
        // amount: 0 = no change, 1 = full spread
        amount = amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount);

        for(auto& t : mTaps){
            // phase: full decorrelation
            t.lfoPhase = gam::rnd::uni(float(2.0 * M_PI));

            // rate: ±15% clustered (add2 is smoother than uni)
            float rateScale =
                1.f + gam::rnd::add2(0.15f * amount);
            t.lfoInc *= rateScale;

            // depth: ±30% triangular (avoids extremes)
            float depthScale =
                1.f + gam::rnd::tri(0.3f * amount);
            t.modDepthSamples *= depthScale;

            // base delay: ±10% triangular
            float delayScale =
                1.f + gam::rnd::tri(0.1f * amount);
            t.baseDelaySamples *= delayScale;
        }
    }

private:
    Array<Tap> mTaps;

    Tp mDry = Tp(1);
    Tp mWet = Tp(0);
    Tp mGlobalFbk = Tp(0);

    void normalizeGains(){
        if(mTaps.size() == 0) return;
        Tp g = Tp(1) / Tp(std::sqrt(double(mTaps.size())));
        for(auto& t : mTaps){
            t.gain = g;
        }
    }
};

} // namespace gam
