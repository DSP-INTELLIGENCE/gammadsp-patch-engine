#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class CharacterLayer {
public:
    // ----- Controls -----
    void setSaturation(Sample s) { sat = std::max((Sample)0, s); }
    void setHysteresis(Sample h) { hysteresis = std::clamp(h, (Sample)0, (Sample)0.999); }
    void setDetectorCurve(Sample c) { curve = std::clamp(c, (Sample)0.2, (Sample)4); }

    void setSoftClip(Sample s) { softClip = std::max((Sample)0, s); }

    void reset() { memory = (Sample)0; }

    // Input: detector envelope (linear)
    Sample process(Sample env)
    {
        // Nonlinear loudness perception
        env = std::pow(env, curve);

        // Hysteresis (envelope memory)
        memory = hysteresis * memory + ((Sample)1 - hysteresis) * env;

        // Saturation on envelope
        if (sat > (Sample)0)
            memory = std::tanh(memory * (Sample)(1 + sat));

        return memory;
    }

    // Output stage soft clipping
    Sample clip(Sample x) const
    {
        if (softClip <= (Sample)0) return x;
        return std::tanh(x * (Sample)(1 + softClip));
    }

private:
    Sample sat        { (Sample)0 };
    Sample hysteresis { (Sample)0 };
    Sample curve      { (Sample)1 };
    Sample softClip   { (Sample)0 };

    Sample memory { (Sample)0 };
};
