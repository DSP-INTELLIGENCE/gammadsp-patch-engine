#pragma once
#include <cmath>
#include "Engine.hpp"
#include "Parameters.hpp"
#include "Hilbert.hpp"

class HilbertFrequencyShifter : public Function {
public:
    HilbertFrequencyShifter(float shiftHz = 100.0f)
    : mShift(shiftHz) {}

    void setSampleRate(float sr) {
        sampleRate = sr;
        updateOsc();
    }

    void setShift(float hz) {
        mShift = hz;
        updateOsc();
    }

    void reset() {
        phase = 0.0f;
        hilbert.reset();
    }

    float process(float x) override {
        float i, q;
        hilbert.run(&x, &i, &q, 1);

        float c = std::cos(phase);
        float s = std::sin(phase);

        // Single-sideband modulation
        float y = i * c - q * s;

        phase += phaseInc;
        if (phase >= 2 * M_PI) phase -= 2 * M_PI;

        return y;
    }

    void run(const float* in, float* out, size_t n) {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    void updateOsc() {
        phaseInc = 2.0f * M_PI * mShift / sampleRate;
    }

    float sampleRate = 48000.0f;
    float mShift = 100.0f;

    float phase = 0.0f;
    float phaseInc = 0.0f;

    Hilbert hilbert;
};
