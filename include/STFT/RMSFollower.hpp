// GDSP/Analysis/RMSFollower.hpp
#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>

// ------------------------------------------------------------
// RMSFollower
// ------------------------------------------------------------
// Sliding-window RMS envelope follower.
// • Real-time safe (no allocation in process())
// • RAII-managed memory
// • Explicit sample-rate dependency
// ------------------------------------------------------------
class RMSFollower: public Function {
public:
    // windowSeconds = RMS window length in seconds
    RMSFollower(float sampleRate, float windowSeconds)
    : sampleRate_(sampleRate)
    {
        setWindow(windowSeconds);
    }

    // Change RMS window length (seconds)
    // ⚠️ Call only at setup time, not in audio thread
    void setWindow(float seconds)
    {
        const unsigned newSize =
            std::max(1u, static_cast<unsigned>(seconds * sampleRate_));

        if (newSize == windowSize_)
            return;

        buffer_.assign(newSize, 0.0f);
        windowSize_ = newSize;

        index_ = 0;
        sumSquares_ = 0.0f;
        rms_ = 0.0f;
    }

    void reset()
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        index_ = 0;
        sumSquares_ = 0.0f;
        rms_ = 0.0f;
    }

    // Per-sample RMS update
    float process(float input)
    {
        const float x2 = input * input;

        sumSquares_ -= buffer_[index_];
        buffer_[index_] = x2;
        sumSquares_ += x2;

        index_++;
        if (index_ >= windowSize_)
            index_ = 0;

        rms_ = std::sqrt(sumSquares_ / static_cast<float>(windowSize_));
        return rms_;
    }

    // Block helper
    void run(const float* input, float* output, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
            output[i] = process(input[i]);
    }

    float value() const { return rms_; }
    unsigned windowSize() const { return windowSize_; }

private:
    float sampleRate_;

    unsigned windowSize_ = 1;
    unsigned index_ = 0;

    float sumSquares_ = 0.0f;
    float rms_ = 0.0f;

    std::vector<float> buffer_;
};
