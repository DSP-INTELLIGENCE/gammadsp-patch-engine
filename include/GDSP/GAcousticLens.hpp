#pragma once
#include "AllPass1.hpp"
#include "FrequencyShifter.hpp"
#include "ModFM.hpp"
class AcousticLens : public TFunction<float> {
public:
    AcousticLens()
    {
        for(int i=0;i<6;i++)
            warp.emplace_back(700.f + i * 300.f);
    }

    void setFocus(float f) { focus = f; }     // 0..1
    void setDepth(float d) { depth = d; }     // 0..1

    float process(float x) noexcept override final
    {
        float y = x;

        for(auto& a : warp)
        {
            float f = 300.f + focus * 4000.f;
            a.freq(f);
            y = a.process(y);
        }

        float refraction = (focus - 0.5f) * 40.f;
        shifter.setShift(refraction);
        y = shifter.process(y);

        modFM.setDepth(depth * 0.02f);
        y = modFM.process(y);

        return y;
    }

private:
    std::vector<AllPass1> warp;
    FrequencyShifter shifter;
    ModFM modFM;

    float focus = 0.5f;
    float depth = 0.3f;
};
