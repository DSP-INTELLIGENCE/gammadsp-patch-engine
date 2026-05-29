#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

class BufferManager
{
public:
    void prepare(double sampleRate, double maxDelaySeconds)
    {
        sr = sampleRate;

        const int required = static_cast<int>(std::ceil(maxDelaySeconds * sr)) + guard;
        allocate(required);
    }

    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    // Safe realtime resize (no audio thread allocation)
    void requestResize(double newMaxDelaySeconds)
    {
        const int newSize = static_cast<int>(std::ceil(newMaxDelaySeconds * sr)) + guard;
        pendingSize = newSize;
        resizeRequested = true;
    }

    // Call once per audio block (NOT per sample)
    void serviceResize()
    {
        if (!resizeRequested) return;

        if (pendingSize <= allocated) return;

        // allocate new memory off audio thread before swapping
        staging.resize(pendingSize, 0.0f);

        // copy existing content into staging
        for (int i = 0; i < allocated; ++i)
            staging[i] = buffer[i];

        buffer.swap(staging);
        allocated = pendingSize;

        resizeRequested = false;
    }

    inline float* data()             { return buffer.data(); }
    inline const float* data() const { return buffer.data(); }
    inline int size() const          { return allocated; }

private:
    void allocate(int size)
    {
        allocated = size;
        buffer.assign(size, 0.0f);
        staging.reserve(size);
    }

private:
    static constexpr int guard = 8;

    std::vector<float> buffer;
    std::vector<float> staging;

    int allocated = 0;
    int pendingSize = 0;
    bool resizeRequested = false;

    double sr = 48000.0;
};
