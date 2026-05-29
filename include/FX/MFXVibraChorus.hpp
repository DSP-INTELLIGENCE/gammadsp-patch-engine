#pragma once
#include "GDSP_ModDelay.hpp"

class ModVibraChorus : public Function
{
public:
    enum Mode { PARALLEL, SERIES };

    ModVibraChorus()
    {
        setMix(0.5f);
    }

    void setMix(float m) { mix = std::clamp(m, 0.0f, 1.0f); }
    void setMode(Mode m){ mode = m; }

    float process(float input) override
    {
        float wet;

        if (mode == PARALLEL)
        {
            float v = _vibrato.process(input);
            float c = _chorus.process(input);
            wet = 0.5f * (v + c);     // normalized
        }
        else // SERIES
        {
            wet = _chorus.process(_vibrato.process(input));
        }

        return input * (1.0f - mix) + wet * mix;
    }

private:
    Mode mode = PARALLEL;
    float mix = 0.5f;

    ModChorus  _chorus;
    ModVibrato _vibrato;
};
