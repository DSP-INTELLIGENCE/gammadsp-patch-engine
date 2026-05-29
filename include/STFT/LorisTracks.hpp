// LorisTracks.hpp
#pragma once

#include <cstdio>
#include <vector>
#include <cstddef>
#include <algorithm>

// ============================================================
// Track data model
// ============================================================

struct TrackBreakpoint {
    double time;       // seconds (absolute)
    double frequency;  // Hz
    double amplitude;  // linear
    double bandwidth;  // Loris bandwidth
    double phase;      // radians
};

struct PartialTrack {
    int label = 0;
    double startTime = 0.0;
    double endTime   = 0.0;
    std::vector<TrackBreakpoint> breakpoints;
};

struct TrackModel {
    double sampleRate = 0.0;
    std::vector<PartialTrack> tracks;
};

// ============================================================
// Runtime cursor for interpolating a track
// ============================================================

struct TrackCursor
{
    const PartialTrack* track = nullptr;
    size_t idx = 0;

    // True only while interpolation is valid and time is in range
    bool active(double t) const
    {
        if (!track || idx + 1 >= track->breakpoints.size())
            return false;

        return t >= track->startTime && t <= track->endTime;
    }

    void advance(double t)
    {
        if (!track) return;

        while (idx + 1 < track->breakpoints.size() &&
               t > track->breakpoints[idx + 1].time)
            ++idx;
    }

    void sample(double t, float& freq, float& amp) const
    {
        const auto& b0 = track->breakpoints[idx];
        const auto& b1 = track->breakpoints[idx + 1];

        double dt = b1.time - b0.time;
        double u  = (dt > 0.0) ? (t - b0.time) / dt : 0.0;
        u = std::clamp(u, 0.0, 1.0);

        freq = float((1.0 - u) * b0.frequency + u * b1.frequency);
        amp  = float((1.0 - u) * b0.amplitude + u * b1.amplitude);
    }
};

// ============================================================
// Serialization
// ============================================================

bool loadTrackModelJson(const char* path, TrackModel& model);
