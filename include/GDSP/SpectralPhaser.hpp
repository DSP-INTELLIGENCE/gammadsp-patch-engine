#pragma once
#include <cmath>
#include <vector>
#include "AllPass1.hpp"
#include "FX/FrequencyShifter.hpp"

class SpectralPhaser : public Function {
public:
    SpectralPhaser(int stages = 6)
    : shifter(0.f)
    {
        for(int i=0;i<stages;i++)
            ap.emplace_back(600.f + i * 250.f);
    }

    void setRate(float r)  { rate = r; }
    void setDepth(float d) { depth = d; }
    void setFeedback(float f) { feedback = std::clamp(f, 0.f, 0.98f); }

    float process(float x) override
    {
        float m = 0.5f + 0.5f * std::sin(phase);
        phase += 2.f * M_PI * rate / gam::sampleRate();

        float y = x;

        for(auto& s : ap)
        {
            float f = 300.f + m * 2500.f * depth;
            s.freq(f);
            y = s.process(y);
        }

        shifter.setShift((m - 0.5f) * 30.f * depth);
        float z = shifter.process(y);

        fb = fb * feedback + z;
        return x + fb;
    }

private:
    std::vector<AllPass1> ap;
    FrequencyShifter shifter;

    float rate = 0.2f;
    float depth = 1.0f;
    float feedback = 0.6f;

    float phase = 0.f;
    float fb = 0.f;
};
