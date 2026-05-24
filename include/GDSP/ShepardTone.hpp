#pragma once
#include "FrequencyShifer.hpp"

class ShepardTone : public Function {
public:
    ShepardTone(int partials = 12, float rate = 10.f)
    : mRate(rate), shifter(rate)
    {
        for(int i = 0; i < partials; ++i)
            phases.push_back((float)i / partials * 2.f * M_PI);
    }

    void setRate(float hz) { mRate = hz; shifter.setShift(hz); }

    float process(float) override
    {
        const float sr = gam::sampleRate();

        float out = 0.f;
        float sum = 0.f;

        for(size_t i = 0; i < phases.size(); ++i)
        {
            float f = 110.f * std::pow(2.f, i / (float)phases.size());
            phases[i] += 2.f * M_PI * f / sr;
            if (phases[i] > 2.f * M_PI) phases[i] -= 2.f * M_PI;

            float s = std::sin(phases[i]);

            // Gaussian window in log-frequency
            float pos = i / (float)phases.size();
            float env = std::exp(-12.f * (pos - 0.5f) * (pos - 0.5f));

            out += s * env;
            sum += env;
        }

        return shifter.process(out / sum);
    }

private:
    float mRate;
    std::vector<float> phases;
    FrequencyShifter shifter;
};
