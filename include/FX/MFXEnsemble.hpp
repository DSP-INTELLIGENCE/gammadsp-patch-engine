#pragma once
#include "GDSP_ModDelay.hpp"

class ModEnsemble : public Function
{
public:
    ModEnsemble()
    {
        for (int i = 0; i < 3; ++i)
        {
            voices[i].setDelay(0.020f);
            voices[i].setFM(0.0f);
            voices[i].setMIXM(0.6f);
            voices[i].setAM(1.0f);
            voices[i].setDepth(0.25f);
            voices[i].setLFO(0.25f);
        }
    }

    void setRate(float r)
    {
        for (auto& v : voices)
            v.setLFO(r);
    }

    void setDepth(float d)
    {
        for (auto& v : voices)
            v.setDepth(d);
    }

    float process(float input) override
    {
        float sum = 0.0f;

        for (int i = 0; i < 3; ++i)
            sum += voices[i].process(input);

        return sum / 3.0f;
    }

private:
    ModChorus voices[3];
};
