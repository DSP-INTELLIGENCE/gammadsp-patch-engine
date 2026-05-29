#pragma once
#include <cmath>
#include <algorithm>

// True stereo infinite rotor (barber-pole spectral rotation)
class BarberPoleStereoRotor
{
public:
    BarberPoleStereoRotor()
    {
        setRate(0.08f);        // Hz
        setDepth(1.0f);        // 0..1
        setBaseShift(15.0f);   // Hz
        setWidth(120.0f);      // Hz
        setStereoWidth(1.0f);  // 0..1
        setMix(1.0f);          // 0..1
        setSmoothingMs(20.0f);
    }

    // ---------- Musical controls ----------
    void setRate(float hz)        { rate.set(std::max(0.001f, hz)); }
    void setDepth(float d)        { depth = std::clamp(d, 0.f, 1.f); }
    void setBaseShift(float hz)   { baseShift = std::max(0.f, hz); }
    void setWidth(float hz)       { width = std::max(0.f, hz); }
    void setMix(float m)          { mix = std::clamp(m, 0.f, 1.f); }
    void setStereoWidth(float w)  { stereoWidth = std::clamp(w, 0.f, 1.f); }

    // smoothing for control signal (ms)
    void setSmoothingMs(float ms)
    {
        float sr = (float)gam::sampleRate();
        ms = std::max(0.0f, ms);
        float samples = std::max(1.f, ms * 0.001f * sr);
        // one-pole coefficient: y = (1-a)*x + a*y
        smoothA = std::exp(-1.0f / samples);
    }

    void reset()
    {
        phase = 0.f;
        ctrlL = ctrlR = 0.f;        
    }

    // ---------- DSP ----------
    StereoSample process(float x)
    {
        // 1) advance rotor phase
        float r = rate.process();
        phase += r / (float)gam::sampleRate();
        phase -= std::floor(phase); // wrap 0..1

        // 2) build *two* barber-pole controls in quadrature (90° offset)
        // saw in [-1,1]
        float sawL = 2.f * phase - 1.f;
        float pR   = phase + 0.25f;          // +90°
        pR -= std::floor(pR);
        float sawR = 2.f * pR - 1.f;

        // map to ctrl in [0,1] with depth
        float cL = (sawL * depth + 1.f) * 0.5f;
        float cR = (sawR * depth + 1.f) * 0.5f;

        // smooth controls (clickless)
        ctrlL = (1.f - smoothA) * cL + smoothA * ctrlL;
        ctrlR = (1.f - smoothA) * cR + smoothA * ctrlR;

        // 3) per-channel shift amount (Hz)
        float shiftHzL = baseShift + ctrlL * width;
        float shiftHzR = baseShift + ctrlR * width;

        float maxHz = 0.45f * (float)gam::sampleRate();
        shiftHzL = std::min(shiftHzL, maxHz);
        shiftHzR = std::min(shiftHzR, maxHz);

        // 4) apply barber-pole spectral shifters (Up + Down with window crossfade)
        upL.freq(+shiftHzL);    downL.freq(-shiftHzL);
        upR.freq(+shiftHzR);    downR.freq(-shiftHzR);

        float yUpL   = upL(x);
        float yDownL = downL(x);
        float yUpR   = upR(x);
        float yDownR = downR(x);

        // barber-pole window: sin(pi * ctrl) crossfade hides wrap
        float wUpL   = std::sin(ctrlL * 3.14159265359f);
        float wDownL = std::sin((1.f - ctrlL) * 3.14159265359f);
        float wUpR   = std::sin(ctrlR * 3.14159265359f);
        float wDownR = std::sin((1.f - ctrlR) * 3.14159265359f);

        float specL = yUpL * wUpL + yDownL * wDownL;
        float specR = yUpR * wUpR + yDownR * wDownR;

        // 5) stereo energy rotation (equal-power) using a *separate* smooth pan control
        // Use sine/cos on the *same* quadrature phase; stereoWidth collapses to center.
        float angL = phase * 6.28318530718f;
        float pan  = std::sin(angL); // [-1,1] rotating pan
        pan *= stereoWidth;

        // equal-power gains from pan in [-1,1]
        // map pan->angle 0..pi/2 where -1 is hard L, +1 hard R
        float pan01 = 0.5f * (pan + 1.f);
        float panAng = pan01 * 1.57079632679f;
        float gL = std::cos(panAng);
        float gR = std::sin(panAng);

        float wetL = specL * gL;
        float wetR = specR * gR;

        // 6) equal-power wet/dry mix (applied per channel)
        float mAng = mix * 1.57079632679f;
        float dG = std::cos(mAng);
        float wG = std::sin(mAng);

        StereoSample out;
        out.outL = x * dG + wetL * wG;
        out.outR = x * dG + wetR * wG;
        return out;
    }

    // convenience buffer runner (mono in → stereo out)
    void run(const float* input, float* outL, float* outR, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            StereoSample s = process(input[i]);
            outL[i] = s.outL;
            outR[i] = s.outR;
        }
    }

    Modulator rate;
    
private:
    // modulation
    
    float depth = 1.0f;

    // barber-pole geometry
    float baseShift = 15.0f;
    float width     = 120.0f;

    // stereo / mix
    float stereoWidth = 1.0f;
    float mix         = 1.0f;

    // state
    float phase  = 0.f;
    float ctrlL  = 0.f;
    float ctrlR  = 0.f;
    float smoothA = 0.98f;

    // four shifters (Up/Down per channel)
    gam::FreqShift<double> upL, downL;
    gam::FreqShift<double> upR, downR;
};
