#pragma once
#include <algorithm>
#include <cmath>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

#include "GDSP_FormantMorphFilter.hpp"
#include "GDSP_NonlinearModalResonator.hpp"
#include "GDSP_DispersionNetwork.hpp"
#include "GDSP_PhaseDiffuser.hpp"
#include "Gamma/FormantData.h"

// Lip radiation model
class LipRadiation {
public:
    void reset() { z = 0.f; }
    void setCoeff(float a) { alpha = std::clamp(a, 0.f, 0.9999f); }
    float process(float x) {
        float y = x - alpha * z;
        z = x;
        return y * 0.5f;
    }
private:
    float z = 0.f;
    float alpha = 0.95f;
};


class VocalTractModel : public Function
{
public:
    VocalTractModel()
    {
        setIM(1.f);
        setAM(1.f);

        // Default vowel: male "AH"
        formant.setCurrentVowel(gam::Vowel::MAN, gam::Vowel::HOD);
        formant.setTargetVowel (gam::Vowel::MAN, gam::Vowel::HOD);
        formant.setResonance(8.0f);
        formant.setMorphSpeed(0.002f);

        // Throat / body behavior
        throat.resize(6);
        throat.setDrive(1.1f);
        throat.setCoupling(0.25f);
        throat.setMix(1.0f);

        dispersion.setDispersion(0.4f);
        dispersion.setFeedback(0.25f);
        dispersion.setDepth(0.25f);
        dispersion.setMix(1.0f);

        diffuser = PhaseDiffuser(8);
        diffuser.setDepth(0.6f);
        diffuser.setMix(1.0f);

        lips.setCoeff(0.95f);
    }

    // -------- Vowel control --------
    void setVowel(gam::Vowel::Voice v, gam::Vowel::Phoneme p)
    {
        formant.setTargetVowel(v, p);
    }

    void setVowelSpeed(float s)
    {
        formant.setMorphSpeed(std::clamp(s, 0.0001f, 0.05f));
    }

    // -------- Expression controls --------
    void setGrowl(float g)
    {
        throat.setDrive(1.f + g * 3.f);
        throat.setCoupling(0.2f + g * 0.6f);
    }

    void setBreath(float b)
    {
        diffuser.setDepth(0.3f + b * 0.7f);
    }

    void setBrightness(float b)
    {
        dispersion.setDispersion(0.2f + b * 0.6f);
    }

    void setMouth(float m)
    {
        lips.setCoeff(0.90f + m * 0.09f);
    }

    // -------- Processing --------
    float process(float x) override
    {
        float in = x * im.process();

        // 1) Vocal formants
        float y = formant.process(in);

        // 2) Throat nonlinear resonance
        y = throat.process(y);

        // 3) Dispersion & airflow smear
        y = dispersion.process(y);

        // 4) Diffusion (air turbulence)
        y = diffuser.process(y);

        // 5) Lip radiation
        y = lips.process(y);

        y *= am.process();
        return y;
    }

private:
    FormantMorphFilter       formant;
    NonlinearModalResonator  throat;
    DispersionNetwork       dispersion;
    PhaseDiffuser           diffuser;
    LipRadiation            lips;

    Modulator im, am;
};
