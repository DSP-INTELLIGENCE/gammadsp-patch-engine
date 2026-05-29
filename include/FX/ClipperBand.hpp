#pragma once
#include <cmath>

// ------------------------------------------------------------
// Clipper band (hard / soft / cubic)
// ------------------------------------------------------------
struct ClipperBand
{
    enum Mode {
        HARD,
        SOFT,
        CUBIC
    };

    // -------- Parameters --------
    Mode  mode      = HARD;
    float threshold = 1.0f;    // linear
    float makeupDB  = 0.0f;
    bool  bypass    = false;

    float process(float x)
    {
        if (bypass)
            return x;

        float y = x;

        switch (mode)
        {
            case HARD:
                y = hardClip(x);
                break;

            case SOFT:
                y = softClip(x);
                break;

            case CUBIC:
                y = cubicClip(x);
                break;
        }

        y *= dbToLin(makeupDB);
        return y;
    }

    static float hardClip(float x, float t = 1.0f)
    {
        if (x >  t) return  t;
        if (x < -t) return -t;
        return x;
    }

    float hardClip(float x) const
    {
        return hardClip(x, threshold);
    }

    float softClip(float x) const
    {
        // tanh soft clip, normalized at threshold
        float a = x / threshold;
        return threshold * std::tanh(a);
    }

    float cubicClip(float x) const
    {
        // cubic polynomial soft clip
        float a = x / threshold;

        if (std::abs(a) >= 1.0f)
            return (a > 0.0f ? threshold : -threshold);

        // y = x - x^3/3
        return threshold * (a - (a * a * a) / 3.0f);
    }

    static float dbToLin(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }
};
