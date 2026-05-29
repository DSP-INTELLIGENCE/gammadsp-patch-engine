// RoomAcoustics.hpp
#pragma once
#include <array>
#include <algorithm>

struct RT60Bands {
    float low  = 0.5f;
    float mid  = 0.5f;
    float high = 0.5f;
};

static inline float clamp01(float v){ return std::clamp(v, 0.f, 1.f); }

static inline RT60Bands computeRT60_Sabine(
    float W, float H, float D,
    const std::array<WallMaterial,6>& mats
){
    // surfaces: 0 left, 1 right, 2 floor, 3 ceiling, 4 front, 5 back
    const float S_leftRight   = H * D;
    const float S_floorCeil   = W * D;
    const float S_frontBack   = W * H;

    const float V = std::max(0.001f, W * H * D);

    auto bandRT60 = [&](auto bandSelector)->float {
        // absorption coefficient alpha = 1 - reflectivity
        float a0 = 1.f - clamp01(bandSelector(mats[0])); // left
        float a1 = 1.f - clamp01(bandSelector(mats[1])); // right
        float a2 = 1.f - clamp01(bandSelector(mats[2])); // floor
        float a3 = 1.f - clamp01(bandSelector(mats[3])); // ceil
        float a4 = 1.f - clamp01(bandSelector(mats[4])); // front
        float a5 = 1.f - clamp01(bandSelector(mats[5])); // back

        float A =
            S_leftRight * (a0 + a1) +
            S_floorCeil * (a2 + a3) +
            S_frontBack * (a4 + a5);

        // prevent blowups for near-zero absorption
        A = std::max(0.01f, A);

        float rt60 = 0.161f * (V / A);

        // practical clamp for typical algorithmic reverb usage
        return std::clamp(rt60, 0.1f, 30.f);
    };

    RT60Bands out;
    out.low  = bandRT60([](const WallMaterial& m){ return m.low;  });
    out.mid  = bandRT60([](const WallMaterial& m){ return m.mid;  });
    out.high = bandRT60([](const WallMaterial& m){ return m.high; });
    return out;
}

static inline float feedbackFromRT60(float rt60Seconds, float loopSeconds)
{
    rt60Seconds = std::max(0.05f, rt60Seconds);
    loopSeconds = std::max(0.001f, loopSeconds);

    // g = 10^(-3 * loop / RT60)
    float g = std::pow(10.f, (-3.f * loopSeconds) / rt60Seconds);
    return std::clamp(g, 0.f, 0.99f);
}
