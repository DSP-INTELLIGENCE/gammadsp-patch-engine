#pragma once
#include <cmath>
#include <vector>

class FrequencyShifter {
public:
    void setSampleRate(float sr) {
        sampleRate = sr;
        updatePhaseInc();
    }

    void setShift(float hz) {
        shift = hz;
        updatePhaseInc();
    }

    void init() {
        // 31-tap Hilbert FIR
        const float coeffs[31] = {
            -0.0180, 0, -0.0225, 0, -0.0285, 0, -0.0372, 0,
            -0.0533, 0, -0.0890, 0, -0.3180, 0, 0.3180, 0,
            0.0890, 0, 0.0533, 0, 0.0372, 0, 0.0285, 0,
            0.0225, 0, 0.0180, 0, 0, 0, 0
        };

        taps.assign(coeffs, coeffs + 31);
        delay.assign(31, 0.0f);
    }

    inline float process(float x) {
        // Hilbert transform
        for(int i = delay.size()-1; i > 0; --i)
            delay[i] = delay[i-1];
        delay[0] = x;

        float h = 0.0f;
        for(size_t i = 0; i < taps.size(); ++i)
            h += delay[i] * taps[i];

        // Complex modulation
        float c = cosf(phase);
        float s = sinf(phase);

        float y = x * c - h * s;

        phase += phaseInc;
        if(phase > 2*M_PI) phase -= 2*M_PI;

        return y;
    }

private:
    void updatePhaseInc() {
        phaseInc = 2 * M_PI * shift / sampleRate;
    }

    float sampleRate = 48000.0f;
    float shift = 100.0f;
    float phase = 0.0f;
    float phaseInc = 0.0f;

    std::vector<float> taps;
    std::vector<float> delay;
};
