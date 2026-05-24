#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>

// Assumes: gam::sampleRate() exists
#include "Engine.hpp"
#include "Parameters.hpp"

// -------------------------------------------------------------
// Autonomous Modulation Engine
// - Generates control signals and writes them into Modulator buses.
// - Modulator::set(x) expects the incoming control value (e.g. -1..1, 0..1).
// - This engine does NOT assume oscillators live inside Modulator.
// -------------------------------------------------------------
class AutonomousModEngine
{
public:
    // Common waveform types
    enum class Wave { Sine, Tri, Saw, Square, SmoothNoise };

    // A single modulation source (generator)
    struct Source
    {
        // --- high-level config ---
        Wave  wave      = Wave::Sine;
        float rateHz    = 0.2f;     // cycles per second (for periodic sources)
        float shape     = 0.0f;     // waveform shaping (meaning depends on wave)
        float depth     = 1.0f;     // output amplitude scaler
        float offset    = 0.0f;     // output DC offset
        bool  bipolar   = true;     // true => [-1,1], false => [0,1]
        bool  enabled   = true;

        // --- internal state ---
        float phase     = 0.0f;     // [0,1)
        float value     = 0.0f;     // last output (post mapping)
        float smooth    = 0.0f;     // smoothing state (for smooth noise, etc.)

        // S&H / noise / chaos state
        uint32_t rng    = 0x12345678u;
        float held      = 0.0f;
        float holdT     = 0.0f;     // seconds until next sample

        // Random walk state
        float walk      = 0.0f;

        // Chaos (logistic map)
        float chaosX    = 0.37f;
        float chaosR    = 3.85f;    // [3.57..4] chaotic region

        // Envelope follower (optional)
        float env       = 0.0f;
        float envCoef   = 0.001f;   // set by setEnvTime()
        float envDrive  = 1.0f;     // how much input drives env
    };

    // Routing from sources to buses
    struct Route
    {
        int src = 0;
        Modulator* dst = nullptr;

        // route shaping
        float amount = 1.0f;  // multiplier applied to src output before dst->set()
        float bias   = 0.0f;  // added before set()
        float clampMin = -1.0f;
        float clampMax =  1.0f;

        // optional: one-pole smoothing on the route (prevents zipper when source changes abruptly)
        float smoothCoef = 0.0f; // 0=no smoothing. else exp(-1/(t*sr)) recommended.
        float smoothState = 0.0f;
    };

public:
    AutonomousModEngine()
    {
        // sensible defaults for a small palette
        // 0: slow sine
        sources[0].wave = Wave::Sine; sources[0].rateHz = 0.12f; sources[0].depth = 1.0f;

        // 1: triangle (slightly faster)
        sources[1].wave = Wave::Tri;  sources[1].rateHz = 0.25f; sources[1].depth = 1.0f;

        // 2: sample & hold
        sources[2].wave = Wave::SmoothNoise; sources[2].rateHz = 0.35f; sources[2].depth = 1.0f;

        // 3: random walk
        sources[3].wave = Wave::SmoothNoise; sources[3].rateHz = 0.08f; sources[3].depth = 1.0f;

        // 4: chaos
        sources[4].wave = Wave::SmoothNoise; sources[4].rateHz = 0.18f; sources[4].depth = 1.0f;

        // Envelope follower setup for any source you choose to use as env
        for (auto& s : sources) setEnvTime(s, 0.05f);
    }

    // Access
    Source& getSource(int i) { return sources[clampIndex(i)]; }
    const Source& getSource(int i) const { return sources[clampIndex(i)]; }

    // Add/remove routes
    int addRoute(int srcIndex, Modulator& dst, float amount = 1.0f, float bias = 0.0f,
                 float clampMin = -1.0f, float clampMax = 1.0f)
    {
        Route r;
        r.src = clampIndex(srcIndex);
        r.dst = &dst;
        r.amount = amount;
        r.bias = bias;
        r.clampMin = clampMin;
        r.clampMax = clampMax;
        routes.push_back(r);
        return (int)routes.size() - 1;
    }

    void clearRoutes() { routes.clear(); }

    // Optional per-route smoothing (time in seconds)
    void setRouteSmoothing(int routeIndex, float timeSec)
    {
        if (routeIndex < 0 || routeIndex >= (int)routes.size()) return;
        timeSec = std::max(0.0f, timeSec);
        if (timeSec <= 0.0f) { routes[routeIndex].smoothCoef = 0.0f; return; }
        routes[routeIndex].smoothCoef = std::exp(-1.0f / (timeSec * (float)gam::sampleRate()));
    }

    // Set env follower time for a source (if you use env mode)
    void setEnvTime(Source& s, float timeSec)
    {
        timeSec = std::clamp(timeSec, 0.001f, 2.0f);
        s.envCoef = std::exp(-1.0f / (timeSec * (float)gam::sampleRate()));
    }

    // Master update (per-sample). Pass an input signal if you want envelope-driven modulation.
    void process(float inputSample = 0.0f)
    {
        const float sr = (float)gam::sampleRate();
        const float invSr = 1.0f / std::max(1.0f, sr);

        // 1) Update all sources
        for (auto& s : sources)
        {
            if (!s.enabled) { s.value = 0.0f; continue; }

            // envelope follower (always available; only matters if you use it)
            const float inAbs = std::fabs(inputSample) * s.envDrive;
            s.env += (inAbs - s.env) * (1.0f - s.envCoef);

            // advance phase for periodic sources
            s.phase += s.rateHz * invSr;
            if (s.phase >= 1.0f) s.phase -= 1.0f;

            float v = 0.0f;

            switch (s.wave)
            {
                case Wave::Sine:   v = std::sin(s.phase * 6.2831853f); break;
                case Wave::Tri:    v = tri(s.phase); break;
                case Wave::Saw:    v = 2.0f * s.phase - 1.0f; break;
                case Wave::Square: v = (s.phase < 0.5f) ? 1.0f : -1.0f; break;

                case Wave::SmoothNoise:
                default:
                    // Smooth noise / S&H hybrid:
                    // - 'rateHz' controls how often a new random target is chosen.
                    // - 'shape' controls smoothing: 0 => S&H, 1 => very smooth.
                    v = smoothNoise(s, invSr);
                    break;
            }

            // optional shaping (simple “curvature”)
            // shape in [-1,1] where + makes it more “peaky”, - makes it softer
            v = curve(v, s.shape);

            // bipolar/unipolar mapping
            if (!s.bipolar)
                v = 0.5f * (v + 1.0f); // [-1,1] -> [0,1]

            // apply depth/offset
            v = v * s.depth + s.offset;

            // clamp to expected range
            if (s.bipolar) v = std::clamp(v, -1.0f, 1.0f);
            else           v = std::clamp(v,  0.0f, 1.0f);

            s.value = v;
        }

        // 2) Apply routing: write into Modulator buses via set()
        for (auto& r : routes)
        {
            if (!r.dst) continue;
            float v = sources[r.src].value * r.amount + r.bias;
            v = std::clamp(v, r.clampMin, r.clampMax);

            // optional smoothing per-route
            if (r.smoothCoef > 0.0f)
            {
                r.smoothState += (v - r.smoothState) * (1.0f - r.smoothCoef);
                v = r.smoothState;
            }

            r.dst->set(v);
        }
    }

    // Convenience: get last source value (post mapping)
    float value(int srcIndex) const { return sources[clampIndex(srcIndex)].value; }

private:
    static int clampIndex(int i) { return std::clamp(i, 0, (int)kNumSources - 1); }

    // Triangle wave in [-1,1]
    static float tri(float p)
    {
        float t = (p < 0.5f) ? (p * 2.0f) : (2.0f - p * 2.0f); // [0,1]
        return t * 2.0f - 1.0f;
    }

    // Simple curvature function: keeps sign, changes “ease”
    static float curve(float x, float shape)
    {
        shape = std::clamp(shape, -1.0f, 1.0f);
        // map shape to exponent: [-1..1] -> [0.5..2.0]
        float expn = 1.0f + 0.75f * shape;
        expn = std::clamp(expn, 0.35f, 3.0f);
        float s = (x >= 0.0f) ? 1.0f : -1.0f;
        return s * std::pow(std::fabs(x), expn);
    }

    // xorshift rng -> [0,1)
    static float rand01(uint32_t& state)
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return (x & 0x00FFFFFFu) * (1.0f / 16777216.0f);
    }

    static float randSigned(uint32_t& state)
    {
        return rand01(state) * 2.0f - 1.0f;
    }

    // SmoothNoise:
    // - pick a new target at ~rateHz
    // - interpolate toward it with shape-controlled smoothing
    static float smoothNoise(Source& s, float invSr)
    {
        // update hold timer
        s.holdT -= invSr;
        if (s.holdT <= 0.0f)
        {
            // next update in seconds
            float hz = std::max(0.001f, s.rateHz);
            s.holdT = 1.0f / hz;
            s.held = randSigned(s.rng); // new target
        }

        // shape maps to smoothing amount:
        // shape <= 0  => more S&H like
        // shape >= 1  => very smooth
        float t = std::clamp(0.2f + 0.8f * std::max(0.0f, s.shape), 0.0f, 1.0f);
        float coef = 0.0f;
        if (t > 0.0f)
        {
            // “smoothing time” as fraction of update interval
            float tau = std::max(0.001f, (s.holdT * (0.15f + 1.5f * t)));
            coef = std::exp(-1.0f / (tau * (float)gam::sampleRate()));
        }

        // one-pole toward target
        s.smooth += (s.held - s.smooth) * (1.0f - coef);
        return s.smooth;
    }

private:
    static constexpr size_t kNumSources = 6;

public:
    // Public so you can directly configure sources without setters if you prefer.
    std::array<Source, kNumSources> sources;

    std::vector<Route> routes;
};
