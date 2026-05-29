#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class PitchShifter {
public:
    PitchShifter(int maxDelay = 4096)
    : size(maxDelay),
      buffer(maxDelay, 0.0f),
      window(maxDelay, 0.0f)
    {
        for (int i = 0; i < size; ++i)
            window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / size));

        half = size / 2;
        write = 0;

        read[0] = 0.f;
        read[1] = (float)half;

        phase[0] = 0.f;
        phase[1] = (float)half;
    }

    void setShift(float ratio) {
        ratio = std::max(0.01f, ratio);
        targetRate = 1.0f - ratio;
    }

    float process(float input)
    {
        rate = m_rate.process(rate + 0.001f * (targetRate - rate));

        buffer[write] = input;

        float out = 0.f;

        for (int i = 0; i < 2; ++i)
        {
            read[i] += rate;
            wrap(read[i]);

            phase[i] += 1.f;
            if (phase[i] >= size) phase[i] -= size;

            int ri = (int)read[i];
            int wi = (int)phase[i];

            float y = buffer[(write - ri + size) % size];
            out += y * window[wi];
        }

        write = (write + 1) % size;
        return out;
    }

    Modulator m_rate;

private:
    void wrap(float& p) {
        while (p < 0)      p += size;
        while (p >= size) p -= size;
    }

    int size, half;
    int write = 0;

    std::vector<float> buffer;
    std::vector<float> window;

    float read[2];
    float phase[2];

    float rate = 0.f;
    float targetRate = 0.f;
};
