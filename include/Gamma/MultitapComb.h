#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Containers.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class MultitapComb : public Delay<Tv, Si, Td> {
public:
    using Base = Delay<Tv, Si, Td>;

    struct Tap {
        float delaySamples; // fractional samples
        Tp fbk;
        Tp ffd;
        bool active;
    };

    MultitapComb() {}

    /// Allocate delay buffer and taps (outside audio thread)
    MultitapComb(float maxDelaySeconds, unsigned numTaps){
        Base::maxDelay(maxDelaySeconds, false);
        taps(numTaps);
    }

    unsigned taps() const { return mTaps.size(); }

    /// Resize taps (⚠️ outside audio thread)
    void taps(unsigned n){
        mTaps.resize(n);
        for(auto& t : mTaps){
            t.delaySamples = 0.f;
            t.fbk = Tp(0);
            t.ffd = Tp(0);
            t.active = true;
        }
    }

    /// Set tap delay in fractional samples
    void delaySamplesR(float samples, unsigned tap){
        mTaps[tap].delaySamples = clampDelay(samples);
    }

    void fbk(Tp v, unsigned tap){ mTaps[tap].fbk = v; }
    void ffd(Tp v, unsigned tap){ mTaps[tap].ffd = v; }
    void active(bool v, unsigned tap){ mTaps[tap].active = v; }

    /// Process sample
    Tv operator()(Tv x){
        Tv sumOut(0);
        Tv sumFbk(0);

        for(const auto& t : mTaps){
            if(!t.active) continue;

            Tv d = Base::read(t.delaySamples * Td::ups());

            sumOut += d + x * t.ffd;
            sumFbk += d * t.fbk;
        }

        Base::write(x + sumFbk);
        return sumOut;
    }

protected:
    Array<Tap> mTaps;

    float clampDelay(float s) const {
        float maxS = Base::maxDelay() * Td::spu();
        return scl::clip(s, 0.f, maxS - 1.f);
    }
};

} // namespace gam
