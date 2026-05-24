// Recorder.hpp
#pragma once

#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "Engine.hpp"

// If you include Gamma headers individually elsewhere, you can include Recorder directly:
// #include "Gamma/Recorder.h"


/// GDSP Recorder wrapper around gam::Recorder
///
/// Purpose:
/// - RT-safe capture of mono or stereo audio into an internal ring buffer (audio thread)
/// - Non-RT thread pulls out the latest contiguous block for analysis / visualization / writing
///
/// Threading rules:
/// - write/process/run: audio thread ONLY
/// - read/resize: non-audio thread ONLY
///
/// Notes:
/// - No allocations occur on audio thread (as long as resize() is not called there)
/// - You can arm/disarm recording with enable()
class Recorder {
public:
    Recorder() = default;

    /// Non-RT: create/resize buffers
    Recorder(int channels, int frames = 8192)
    : mRec(channels, frames)
    , mEnabled(true)
    {}

    // -----------------------------
    // Non-RT only
    // -----------------------------

    /// Resize internal ring buffer (NOT RT-safe)
    void resize(int channels, int frames) {
        mRec.resize(channels, frames);
    }

    /// Pull out latest recorded block.
    /// Returns number of frames copied into internal read buffer.
    /// If 0, no new block available and outBuf is left unchanged by gam::Recorder.
    ///
    /// outBuf points to interleaved float samples [frames * channels].
    int read(float*& outBuf) {
        return mRec.read(outBuf);
    }

    // -----------------------------
    // RT-safe controls (atomic)
    // -----------------------------

    void enable(bool b) { mEnabled.store(b, std::memory_order_relaxed); }
    bool enabled() const { return mEnabled.load(std::memory_order_relaxed); }

    // -----------------------------
    // Informational (safe)
    // -----------------------------

    int channels() const { return mRec.channels(); }
    int frames()   const { return mRec.frames(); }
    int size()     const { return mRec.size(); }

    // -----------------------------
    // Audio thread capture APIs
    // -----------------------------

    /// Capture a mono sample into channel 0 (audio thread)
    inline void processMono(float x) {
        if (!enabled()) return;
        // Recorder::write(v, chan=0) writes and advances one frame.
        mRec.write(x, 0);
    }

    /// Capture stereo samples into channels 0/1 (audio thread)
    inline void processStereo(float L, float R) {
        if (!enabled()) return;
        // Recorder has an optimized stereo write(v1,v2,chan=0)
        if (mRec.channels() >= 2) {
            mRec.write(L, R, 0);
        } else {
            // If configured as mono, downmix into ch0
            mRec.write(0.5f * (L + R), 0);
        }
    }

    /// Capture an interleaved block: src = [frames][channels] (audio thread)
    /// Returns number of frames written (per gam::Recorder::write)
    inline int runInterleaved(const float* src, int numFrames) {
        if (!enabled()) return 0;
        return mRec.write(src, numFrames);
    }

    /// Capture planar mono: src[n] (audio thread)
    inline void runMono(const float* src, float* /*unused*/, size_t n) {
        if (!enabled()) return;
        for (size_t i = 0; i < n; ++i) mRec.write(src[i], 0);
    }

    /// Capture planar stereo: L[n], R[n] (audio thread)
    inline void runStereo(const float* L, const float* R, size_t n) {
        if (!enabled()) return;
        if (mRec.channels() >= 2) {
            for (size_t i = 0; i < n; ++i) mRec.write(L[i], R[i], 0);
        } else {
            for (size_t i = 0; i < n; ++i) mRec.write(0.5f * (L[i] + R[i]), 0);
        }
    }

    /// Capture from an upstream mono Function/Generator graph output
    /// This is a convenience sink; it does not fit Generator/Function inheritance on purpose.
    inline void tap(float x) { processMono(x); }

    inline void tap(float L, float R) { processStereo(L, R); }

private:
    gam::Recorder mRec;
    std::atomic<bool> mEnabled{true};
};
