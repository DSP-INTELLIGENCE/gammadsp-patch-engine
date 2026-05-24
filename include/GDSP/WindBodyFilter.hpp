#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "NonlinearModalResonator.hpp"
#include "DispersionNetwork.hpp"
#include "PhaseDiffuser.hpp"

class WindBodyFilter : public Function
{
public:
    WindBodyFilter()
    : mix(1.f)
    {
        setIM(1.f);
        setAM(1.f);

        // configure air-column defaults
        resonator.resize(8);
        resonator.setDrive(0.9f);
        resonator.setCoupling(0.35f);
        resonator.setMix(1.0f);

        dispersion.setDispersion(0.4f);
        dispersion.setFeedback(0.3f);
        dispersion.setDepth(0.25f);
        dispersion.setMix(1.0f);

        diffuser = PhaseDiffuser(8);
        diffuser.setDepth(0.7f);
        diffuser.setMix(1.0f);
    }

    // -------- Controls --------
    void setTubeLength(float l)
    {
        l = std::clamp(l, 0.3f, 2.f);
        for(int i = 0; i < 8; ++i)
            resonator.setMode(i, 90.f * (i+1) / l, 0.998f, 1.f/(i+1));
    }

    void setBore(float b)
    {
        dispersion.setDispersion(0.2f + b * 0.8f);
    }

    void setBreath(float b)
    {
        resonator.setDrive(0.5f + b * 2.0f);
        resonator.setCoupling(0.15f + b * 0.6f);
    }

    void setTone(float t)
    {
        diffuser.setDepth(0.3f + t * 0.7f);
    }

    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        float y = resonator.process(in);
        y = dispersion.process(y);
        y = diffuser.process(y);

        float out = x * (1.f - mix) + y * mix;
        out *= am.process();
        return out;
    }

private:
    NonlinearModalResonator resonator;
    DispersionNetwork dispersion;
    PhaseDiffuser diffuser;

    float mix;
    Modulator im, am;
};
