#pragma once
#include <cmath>
#include <algorithm>
#include "GDSP_Engine.hpp"

struct MasterOutput
{
    // user controls
    float outputGainDB = 0.f;     // -inf..+12 dB
    bool  normalize   = false;

    // target loudness (approx RMS)
    float targetRMS = 0.2f;

    // meters
    float rms = 0.f;
    float peak = 0.f;

    // internal
    float gain = 1.f;
    float rmsEnv = 0.f;

    inline float zap(float x)
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dBToGain(float dB)
    {
        return std::pow(10.f, dB / 20.f);
    }

    void reset()
    {
        rms = peak = 0.f;
        rmsEnv = 0.f;
        gain = 1.f;
    }

    float process(float x)
    {
        // measure
        peak = std::max(peak, std::fabs(x));
        rmsEnv += (x*x - rmsEnv) * 0.001f;
        rms = std::sqrt(rmsEnv);

        // normalization
        if (normalize && rms > 1e-6f)
        {
            float targetGain = targetRMS / rms;
            gain += (targetGain - gain) * 0.001f;
        }
        else
        {
            gain = dBToGain(outputGainDB);
        }

        return zap(x * gain);
    }
};
