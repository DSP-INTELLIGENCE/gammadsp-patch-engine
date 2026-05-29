#pragma once

struct HardClipper
{
    // Controls
    float threshold = 1.f;   // clipping point
    float drive     = 1.f;   // how hard we hit it
    float asym      = 0.f;   // even harmonic bias
    float trim      = 1.f;

    // Smoothing
    float thrLP  = 1.f;
    float drvLP  = 1.f;
    float asymLP = 0.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core clip with soft knee transition
    inline float clipCore(float x)
    {
        float t = thrLP;

        // soft knee around threshold
        float y;
        if (x > t) {
            float d = x - t;
            y = t + d / (1.f + d * d);
        } else if (x < -t) {
            float d = x + t;
            y = -t + d / (1.f + d * d);
        } else {
            y = x;
        }

        // asymmetry
        y += asymLP * y * y;

        return y;
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.3f * drvLP);
    }

    inline float process(float x)
    {
        // smooth controls
        thrLP  += 0.002f * (threshold - thrLP);
        drvLP  += 0.002f * (drive     - drvLP);
        asymLP += 0.002f * (asym      - asymLP);

        // drive in
        float y = x * drvLP;

        // clip
        y = clipCore(y);

        // output trim & loudness
        y *= trim * loudComp();

        return zap(y);
    }
};
