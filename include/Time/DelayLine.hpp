#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class DelayLine
{
public:
    DelayLine() = default;

    void prepare(double sampleRate, double maxDelaySeconds)
    {
        sr = sampleRate;

        const int maxSamples = static_cast<int>(std::ceil(maxDelaySeconds * sr)) + guard;
        buffer.assign(maxSamples, 0.0f);
        size = static_cast<int>(buffer.size());

        writeIndex = 0;
    }

    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    // Write input sample into delay line
    inline void write(float input)
    {
        buffer[writeIndex] = input;
        advanceWrite();
    }

    // Read delayed sample (supports fractional delay)
    inline float read(float delaySamples) const
    {
        float readPos = writeIndex - delaySamples;
        while (readPos < 0.0f) readPos += size;
        while (readPos >= size) readPos -= size;

        int i0 = static_cast<int>(readPos);
        int i1 = (i0 + 1) % size;
        float frac = readPos - i0;

        // Linear interpolation
        float s0 = buffer[i0];
        float s1 = buffer[i1];

        return s0 + frac * (s1 - s0);
    }

private:
    inline void advanceWrite()
    {
        writeIndex++;
        if (writeIndex >= size)
            writeIndex = 0;
    }

private:
    static constexpr int guard = 4;

    std::vector<float> buffer;
    int size = 0;
    int writeIndex = 0;
    double sr = 48000.0;
};
