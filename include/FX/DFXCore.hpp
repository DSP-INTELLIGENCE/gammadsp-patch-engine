#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include "Engine.hpp"
#include "Parameters.hpp"

// ------------------------------------------------------------
// Core utilities (RT safe)
// ------------------------------------------------------------
namespace dfx
{
    static inline float clampf(float x, float a, float b)
    {
        return std::min(std::max(x, a), b);
    }

    static inline float clamp01(float x) { return clampf(x, 0.f, 1.f); }

    // Equal-power dry/wet (mix=0 => dry, mix=1 => wet)
    static inline void equalPowerMix(float dry, float wet, float mix, float& out)
    {
        float m = clamp01(mix);
        float theta = m * float(M_PI_2);
        out = dry * std::cos(theta) + wet * std::sin(theta);
    }

    // Map 0..1 exponentially between [a,b] using curvature k (k>0)
    static inline float expMap01(float n, float a, float b, float k = 3.0f)
    {
        n = clamp01(n);
        float e = (std::exp(k * n) - 1.f) / (std::exp(k) - 1.f);
        return a + (b - a) * e;
    }

    // Smoothstep (nice knob feel)
    static inline float smoothstep01(float x)
    {
        x = clamp01(x);
        return x * x * (3.f - 2.f * x);
    }    

    // Tiny xorshift RNG (for drift/noise), deterministic
    struct XorShift32
    {
        uint32_t s = 0x12345678u;
        inline uint32_t nextU32()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return s;
        }
        inline float nextFloatSigned()
        {
            // [-1,1)
            return float(int32_t(nextU32())) / 2147483648.0f;
        }
    };

    // Soft saturation (tanh) with drive >= 0
    static inline float softsat_tanh(float x, float drive)
    {
        if (drive <= 0.f) return x;
        float k = 1.f + 8.f * drive;
        return std::tanh(k * x) / std::tanh(k);
    }
}

// ------------------------------------------------------------
// DFX Core base class (common plumbing)
// ------------------------------------------------------------
class DFXCore : public Function
{
public:
    DFXCore()
    {
    }

    // Common wet/dry/mix (smoothed, RT-safe)
    void setWet(float w)  { mWet = std::max(0.f, w); }
    void setDry(float d)  { mDry = std::max(0.f, d); }
    void setMix(float m)  { mMix = dfx::clamp01(m); } // 0..1

protected:
    // Call from derived class to apply final mix
    inline float mixOut(float dryIn, float wetIn)
    {
        float wetG = mWet;
        float dryG = mDry;
        float mix  = mMix;

        float dry = dryIn * dryG;
        float wet = wetIn * wetG;

        float out;
        dfx::equalPowerMix(dry, wet, mix, out);
        return out;
    }

private:
    float  mWet, mDry, mMix;    
};

// ------------------------------------------------------------
// Stereo wrapper pattern (no arrays of classes needed)
// ------------------------------------------------------------
struct StereoFrame { float L = 0.f; float R = 0.f; };

// Use this pattern when you want stereo processing in a mono-return engine.
class DFXStereoCore
{
public:
    virtual ~DFXStereoCore() = default;
    virtual StereoSample processStereo(float inL, float inR) = 0;

    // Convenience for mono-in → stereo-out
    inline StereoSample processMonoToStereo(float x)
    {
        return processStereo(x, x);
    }
};
