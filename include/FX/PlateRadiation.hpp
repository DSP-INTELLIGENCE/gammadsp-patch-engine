#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"
#include "NonlinearModalResonator.hpp"
#include "DispersionNetwork.hpp"
#include "PhaseDiffuser.hpp"

// Simple radiation model
class PlateRadiation {
public:
    void set(float a, float g) {
        alpha = std::clamp(a, 0.f, 0.9999f);
        gain  = std::max(0.f, g);
    }
    float process(float x) {
        float y = x - alpha * z;
        z = x;
        return y * gain;
    }
private:
    float z = 0.f;
    float alpha = 0.97f;
    float gain  = 0.6f;
};

class PlateResonator : public Function
{
public:
    PlateResonator()
    : mix(1.f)
    {
        setIM(1.f);
        setAM(1.f);

        modes.resize(24);                 // dense mode field
        modes.setDrive(1.4f);
        modes.setCoupling(0.4f);
        modes.setMix(1.0f);

        dispersion.setDispersion(0.85f);  // strong stiffness
        dispersion.setFeedback(0.5f);
        dispersion.setDepth(0.7f);
        dispersion.setMix(1.0f);

        diffuser = PhaseDiffuser(12);     // scattering field
        diffuser.setDepth(0.9f);
        diffuser.setMix(1.0f);

        rad.set(0.97f, 0.6f);
        setSize(1.0f);
        setMaterial(0.7f);
        setDamping(0.35f);
    }

    // -------- Controls --------
    void setSize(float s)
    {
        size = std::clamp(s, 0.3f, 3.f);
        updateModes();
    }

    void setMaterial(float m)
    {
        material = std::clamp(m, 0.f, 1.f);
        dispersion.setDispersion(0.3f + material * 0.7f);
    }

    void setDamping(float d)
    {
        damping = std::clamp(d, 0.f, 1.f);
        updateModes();
    }

    void setBrightness(float b)
    {
        modes.setDrive(1.f + b * 3.f);
    }

    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        float y = modes.process(in);
        y = dispersion.process(y);
        y = diffuser.process(y);
        y = rad.process(y);

        float out = x * (1.f - mix) + y * mix;
        out *= am.process();
        return out;
    }

private:
    void updateModes()
    {
        float base = 80.f / size;
        float inharm = 1.0f + material * 1.5f;
        float decayBase = 0.9998f - damping * 0.01f;

        for(int i = 0; i < 24; ++i)
        {
            float n = i + 1;
            float f = base * std::pow(n, 1.3f) * std::pow(inharm, 0.3f * n / 24.f);
            float d = std::clamp(decayBase - n * 0.0002f, 0.90f, 0.99999f);
            float g = 1.f / std::pow(n, 0.7f);

            modes.setMode(i, f, d, g);
        }
    }

private:
    NonlinearModalResonator modes;
    DispersionNetwork dispersion;
    PhaseDiffuser diffuser;
    PlateRadiation rad;

    float size = 1.f;
    float material = 0.7f;
    float damping = 0.35f;
    float mix;

    Modulator im, am;
};
