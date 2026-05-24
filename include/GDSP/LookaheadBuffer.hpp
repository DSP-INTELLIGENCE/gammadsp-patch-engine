#pragma once
#include <algorithm>
#include <vector>
#include <cmath>

template <class Sample>
class LookaheadBuffer {
public:
    explicit LookaheadBuffer(int maxSamples)
    : buffer(maxSamples, (Sample)0), size(maxSamples) {}

    void setDelay(int samples) {
        delay = std::clamp(samples, 0, size - 1);
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), (Sample)0);
        write = 0;
    }

    Sample process(Sample x)
    {
        int read = write - delay;
        if (read < 0) read += size;

        Sample y = buffer[read];
        buffer[write] = x;

        write++;
        if (write == size) write = 0;

        return y;
    }

private:
    std::vector<Sample> buffer;
    int write = 0;
    int delay = 0;
    int size;
};
