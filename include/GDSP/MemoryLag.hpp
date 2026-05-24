#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class MemoryLag {
public:
    void setTimeConstant(Sample seconds, Sample sampleRate)
    {
        seconds = std::max(seconds, (Sample)1e-6);
        coeff = std::exp(-(Sample)1 / (seconds * sampleRate));
    }

    void setImmediate()
    {
        coeff = 0;
    }

    void reset(Sample v = 0)
    {
        state = v;
    }

    inline Sample process(Sample input)
    {
        state = state * coeff + input * ((Sample)1 - coeff);
        return state;
    }

private:
    Sample coeff { 0 };
    Sample state { 0 };
};
