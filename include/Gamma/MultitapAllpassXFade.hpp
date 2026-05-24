#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"
#include <cmath>

namespace gam {

struct XFadeState {
    float delayA = 0.f;
    float delayB = 0.f;
    float fade   = 0.f;
    float fadeInc = 1.f / 128.f;
    bool  fading = false;
};

template<
    unsigned N,
    class Tv = gam::real,
    template<class> class Si = ipl::Linear,
    class Tp = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class MultitapAllpassXFade : public Td {
public:
    MultitapAllpassXFade(){
        for(unsigned i=0; i<N; ++i){
            mG[i] = Tp(0.7);
        }
    }

    void maxDelay(float maxDelayUnits){
        for(unsigned i=0; i<N; ++i){
            mD[i].maxDelay(maxDelayUnits, false);
            mD[i].zero();
            mXF[i] = {};
        }
    }

    void fadeTimeSamples(float samples){
        float inc = (samples > 0.f) ? 1.f / samples : 1.f;
        for(unsigned i=0; i<N; ++i)
            mXF[i].fadeInc = inc;
    }

    void set(unsigned stage, float units, Tp g){
        setDelay(stage, units);
        gain(stage, g);
    }

    void setDelay(unsigned stage, float units){
        float s = units * Td::spu();
        auto& xf = mXF[stage];

        if(!xf.fading){
            xf.delayB = s;
            xf.fade   = 0.f;
            xf.fading = true;
        }else{
            xf.delayB = s;
        }
    }

    void gain(unsigned stage, Tp g){
        mG[stage] = scl::clip(g, Tp(-0.999), Tp(0.999));
    }

    void reset(){
        for(unsigned i=0; i<N; ++i)
            mD[i].zero();
    }

    Tv operator()(Tv x){
        for(unsigned i=0; i<N; ++i){
            x = stageProcess(i, x);
        }
        return x;
    }

private:
    Tv stageProcess(unsigned i, Tv x){
        auto& dly = mD[i];
        auto& xf  = mXF[i];
        Tp g = mG[i];

        Tv wdA = dly.read(xf.delayA * Td::ups());
        Tv wdB = dly.read(xf.delayB * Td::ups());

        Tv wd;
        if(xf.fading){
            float f = xf.fade;
            float a = std::cos(f * float(M_PI_2));
            float b = std::sin(f * float(M_PI_2));
            wd = wdA * a + wdB * b;
        }else{
            wd = wdA;
        }

        Tv y = wd - x * g;
        dly.write(x + y * g);

        if(xf.fading){
            xf.fade += xf.fadeInc;
            if(xf.fade >= 1.f){
                xf.fade   = 0.f;
                xf.delayA = xf.delayB;
                xf.fading = false;
            }
        }

        return y;
    }

private:
    Delay<Tv,Si,Td> mD[N];
    Tp              mG[N];
    XFadeState      mXF[N];
};

} // namespace gam
