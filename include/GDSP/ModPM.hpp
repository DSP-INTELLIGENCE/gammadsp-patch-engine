#pragma once
#include <algorithm>
#include <vector>
#include "Engine.hpp"
#include "Parameters.hpp"

class ModPM : public Function
{
public:
    ModPM(float maxDelay = 0.05f)
    : buffer(size_t(gam::sampleRate() * maxDelay) + 2),
      maxDelaySamples(float(buffer.size() - 2))
    {
        setDepth(0.0f);
        setFB(0.0f);
        setIM(1.0f);
        setAM(1.0f);
    }

    // -------- Controls --------
    void setDepth(float d) { depth = std::max(d, 0.0f); }
    void setFB(float f)    { fb = std::clamp(f, 0.0f, 0.99f); }

    void setMM(float v) { mm.set(v); }
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float input)
    {
        float m  = mm.process();      // [-1,1]
        float in = input * im.process();

        // Write input with feedback
        buffer[writeIndex] = in + fb * last;

        // Phase-modulated read index
        float phaseOffset = depth * m * maxDelaySamples;
        float readIndex   = writeIndex - phaseOffset;

        // Wrap
        if (readIndex < 0) readIndex += maxDelaySamples;
        if (readIndex >= maxDelaySamples) readIndex -= maxDelaySamples;

        // Linear interpolation
        int i0 = int(readIndex);
        int i1 = (i0 + 1) % buffer.size();
        float frac = readIndex - i0;

        float y = buffer[i0] * (1.0f - frac) + buffer[i1] * frac;

        // Advance
        writeIndex = (writeIndex + 1) % buffer.size();
        last = y;

        return y * am.process();
    }

private:
    std::vector<float> buffer;
    float maxDelaySamples;

    int writeIndex = 0;

    float depth = 0.0f;
    float fb    = 0.0f;

    Modulator mm;
    Modulator im;
    Modulator am;

    float last = 0.0f;
};
