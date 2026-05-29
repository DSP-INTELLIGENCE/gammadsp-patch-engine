#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"
#include "NonlinearModalResonator.hpp"
#include "DispersionNetwork.hpp"
#include "PhaseDiffuser.hpp"

// Radiation from drum opening
class DrumRadiation {
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
    float alpha = 0.94f;
    float gain  = 0.7f;
};

class DrumShellFilter : public Function
{
public:
    DrumShellFilter()
    : mix(1.f)
    {
        setIM(1.f);
        setAM(1.f);

        // Shell modes: fewer but strong
        shell.resize(8);
        shell.setDrive(1.8f);
        shell.setCoupling(0.5f);
        shell.setMix(1.0f);

        // Air cavity response
        air.resize(4);
        air.setDrive(0.9f);
        air.setCoupling(0.2f);
        air.setMix(1.0f);

        dispersion.setDispersion(0.3f);
        dispersion.setFeedback(0.25f);
        dispersion.setDepth(0.3f);
        dispersion.setMix(1.0f);

        diffuser = PhaseDiffuser(8);
        diffuser.setDepth(0.6f);
        diffuser.setMix(1.0f);

        rad.set(0.94f, 0.7f);

        setSize(1.0f);
        setTension(0.6f);
        setDamping(0.4f);
    }

    // -------- Controls --------
    void setSize(float s)
    {
        size = std::clamp(s, 0.5f, 2.5f);
        updateModes();
    }

    void setTension(float t)
    {
        tension = std::clamp(t, 0.f, 1.f);
        shell.setDrive(1.2f + tension * 2.5f);
    }

    void setDamping(float d)
    {
        damping = std::clamp(d, 0.f, 1.f);
        updateModes();
    }

    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    float process(float x) override
    {
        float in = x * im.process();

        // Shell vibration
        float y = shell.process(in);

        // Air cavity resonance
        y = air.process(y);

        // Structural dispersion
        y = dispersion.process(y);

        // Turbulence / scattering
        y = diffuser.process(y);

        // Radiation
        y = rad.process(y);

        float out = x * (1.f - mix) + y * mix;
        out *= am.process();
        return out;
    }

private:
    void updateModes()
    {
        float base = 90.f / size;
        float decayBase = 0.9995f - damping * 0.01f;

        for(int i = 0; i < 8; ++i)
        {
            float n = i + 1;
            float f = base * std::pow(n, 1.2f);
            float d = std::clamp(decayBase - n * 0.0005f, 0.85f, 0.9999f);
            float g = 1.f / std::pow(n, 0.6f);
            shell.setMode(i, f, d, g);
        }

        // Air cavity (simpler)
        for(int i = 0; i < 4; ++i)
        {
            float n = i + 1;
            float f = base * 0.6f * n;
            float d = std::clamp(decayBase - n * 0.001f, 0.85f, 0.9999f);
            float g = 0.6f / n;
            air.setMode(i, f, d, g);
        }
    }

private:
    NonlinearModalResonator shell;
    NonlinearModalResonator air;
    DispersionNetwork dispersion;
    PhaseDiffuser diffuser;
    DrumRadiation rad;

    float size = 1.f;
    float tension = 0.6f;
    float damping = 0.4f;
    float mix;

    Modulator im, am;
};
