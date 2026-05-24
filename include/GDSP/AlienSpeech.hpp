#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "Biquad.hpp"
#include "FX/FrequencyShifter.hpp"
#include "FX/SpectralPhaser.hpp"

class AlienSpeech : public Function {
public:
    AlienSpeech()
    {
        for(int i=0;i<5;i++)
        {
            float f = 250.f + i * 800.f;
            formants.emplace_back(f, 3.5f, gam::PEAKING);
            drift.emplace_back(0.f);
        }
    }

    float process(float x) override
    {
        float y = x;

        for(size_t i=0;i<formants.size();i++)
        {
            float z = formants[i].process(y);
            drift[i].setShift((i + 1) * 1.5f);
            z = drift[i].process(z);
            y = z;
        }

        y = phaser.process(y);
        return y;
    }

private:
    std::vector<Biquad> formants;
    std::vector<FrequencyShifter> drift;
    SpectralPhaser phaser;
};
