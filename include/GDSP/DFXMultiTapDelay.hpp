// DFXMultiTapDelay.hpp
#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Delay.hpp"
#include "GDSP_Parameters.hpp"

// ----------------------------------------------------
// small helpers
// ----------------------------------------------------
inline float gdsp_clampf(float x, float lo, float hi){
    return std::max(lo, std::min(hi, x));
}

// ----------------------------------------------------
// loop processors (cheap + stable)
// ----------------------------------------------------
struct OnePoleLP {
    float a = 0.0f, y = 0.0f;
    void setCutoffHz(float hz){
        float sr = (float)gam::sampleRate();
        hz = gdsp_clampf(hz, 5.0f, 0.49f * sr);
        float x = hz / (0.5f * sr);
        a = gdsp_clampf(x, 0.0005f, 0.9995f);
    }
    float process(float x){ y += a * (x - y); return y; }
};

struct OnePoleHP {
    OnePoleLP lp;
    void setCutoffHz(float hz){ lp.setCutoffHz(hz); }
    float process(float x){ return x - lp.process(x); }
};

struct SoftClip {
    float drive = 1.0f;
    float process(float x){
        x *= drive;
        return x / (1.0f + std::abs(x));
    }
};

struct LoopLimiter {
    float env = 0.0f, release = 0.9995f;
    float process(float x){
        float e = std::abs(x);
        env = std::max(e, env * release);
        if(env > 1.0f) x *= 1.0f / env;
        return x;
    }
};

// ----------------------------------------------------
// Multi-Tap Modular Delay
// ----------------------------------------------------
class DFXMultiTapDelay : public Function {
public:
    DFXMultiTapDelay(float maxDelaySec = 2.0f, unsigned taps = 4)
    : delay(maxDelaySec, taps, 0.25f)
    {
        delay.setIpolType(ALLPASS);
        setSmoothingMs(15.0f);

        setDamping(100.0f, 6000.0f);
        setFeedback01(0.35f);
        setMix01(0.35f);
    }

    // ---------------- parameters ----------------
    void setTap(unsigned i, float timeSec){ delay.setTapTime(i, timeSec); }

    void setFeedback01(float fb){ pFeedback.set(gdsp_clampf(fb, 0.f, 0.999f)); }
    void setMix01(float m){ pMix.set(gdsp_clampf(m, 0.f, 1.f)); }
    void setInputGain(float g){ pInGain.set(g); }
    void setWetGain(float g){ pWetGain.set(g); }

    void setDamping(float hpfHz, float lpfHz){
        pHPF.set(hpfHz);
        pLPF.set(lpfHz);
    }

    void setSmoothingMs(float ms){
        pFeedback.setTime(ms);
        pMix.setTime(ms);
        pInGain.setTime(ms);
        pWetGain.setTime(ms);
        pHPF.setTime(ms);
        pLPF.setTime(ms);
    }

    // ---------------- audio ----------------
    float process(float input) override {
        const float inGain = pInGain.process();
        const float wetGain = pWetGain.process();
        const float mix = pMix.process();
        const float fb = pFeedback.process();

        hpf.setCutoffHz(pHPF.process());
        lpf.setCutoffHz(pLPF.process());

        float x = input * inGain;

        // ---- read & sum taps ----
        float sum = 0.0f;
        for(unsigned i=0;i<delay.numTaps();++i)
            sum += delay.read(i);

        // ---- feedback loop shaping ----
        float loop = sum;
        loop = hpf.process(loop);
        loop = lpf.process(loop);
        loop = sat.process(loop);
        loop = limiter.process(loop);
        loop *= fb;

        // ---- write back ----
        delay.write(x + loop);

        // ---- output mix ----
        float wet = sum * wetGain;
        return x*(1.0f-mix) + wet*mix;
    }

private:
    MultitapDelay delay;

    ParamLinear pFeedback{0.35f,15.0f};
    ParamLinear pMix{0.35f,15.0f};
    ParamLinear pInGain{1.0f,15.0f};
    ParamLinear pWetGain{1.0f,15.0f};

    ParamLinear pHPF{100.0f,15.0f};
    ParamLinear pLPF{6000.0f,15.0f};

    OnePoleHP hpf;
    OnePoleLP lpf;
    SoftClip sat;
    LoopLimiter limiter;
};
