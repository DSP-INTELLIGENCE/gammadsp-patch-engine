#pragma once
#include <cmath>
#include <cstdlib>

class SidebandGenerator {
public:
    enum ModType {
        SINE,
        SAW,
        SQUARE,
        NOISE
    };

    void setSampleRate(float sr) {
        sampleRate = sr;
    }

    void setFrequency(float f) {
        freq = f;
        phaseInc = 2.0f * M_PI * freq / sampleRate;
    }

    void setDepth(float d) {
        depth = d;
    }

    void setModType(ModType t) {
        type = t;
    }

    inline float process(float carrier) {
        float mod = nextMod();
        return carrier * mod * depth;
    }

private:
    float nextMod() {
        float v = 0.0f;

        switch(type) {
        case SINE:
            v = sinf(phase);
            break;

        case SAW:
            v = 2.0f * (phase / (2.0f * M_PI)) - 1.0f;
            break;

        case SQUARE:
            v = (phase < M_PI) ? 1.0f : -1.0f;
            break;

        case NOISE:
            v = 2.0f * (float(rand()) / RAND_MAX) - 1.0f;
            break;
        }

        phase += phaseInc;
        if(phase >= 2.0f * M_PI)
            phase -= 2.0f * M_PI;

        return v;
    }

    float sampleRate = 48000.0f;
    float freq = 100.0f;
    float phase = 0.0f;
    float phaseInc = 0.0f;
    float depth = 1.0f;
    ModType type = SINE;
};
