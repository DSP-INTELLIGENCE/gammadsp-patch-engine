#pragma once
#include <cmath>
#include <algorithm>
#include "AnalogSupersawOsc.hpp"

class SuperSquareOsc : public AnalogSupersawOsc {
public:
    SuperSquareOsc(float freq = 110.f, unsigned voices = 7)
    : AnalogSupersawOsc(freq, voices) 
    {
        rebuildSquareTable();
    }

    void setPulseWidth(float pw) {
        mPulseWidth = std::clamp(pw, 0.05f, 0.95f);
        rebuildSquareTable();
    }

    void setFreq(float f) {
        mBase = f;
        rebuildSquareTable();
    }

private:
    void rebuildSquareTable() {
        const float nyquist = 0.5f * getSampleRate();
        const float f = mBase;
        const unsigned maxHarm = unsigned(nyquist / f);

        std::vector<float> frame(tableSize(), 0.f);

        for(unsigned k = 1; k <= maxHarm; k += 2) {
            float amp = 4.f / (M_PI * k);
            float phase = k * (0.5f - mPulseWidth) * M_PI * 2.f;

            for(unsigned i = 0; i < frame.size(); ++i) {
                float t = float(i) / frame.size();
                frame[i] += amp * std::sin(2.f * M_PI * k * t + phase);
            }
        }

        normalize(frame);

        for(auto& v : mOscs)
            v.loadWavetable(frame);
    }

    void normalize(std::vector<float>& f) {
        float m = 0.f;
        for(float x : f) m = std::max(m, std::abs(x));
        if(m > 0.f) for(float& x : f) x /= m;
    }

private:
    float mPulseWidth = 0.5f;
};
