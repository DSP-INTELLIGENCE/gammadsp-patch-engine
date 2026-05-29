// SVFMorphFilter.hpp
#pragma once
#include <cmath>
#include <algorithm>

class SVFMorphFilter
{
public:
    SVFMorphFilter(float sampleRate = 48000.0f)
    : fs(sampleRate)
    {
        setCutoff(1000.0f);
        setResonance(0.707f);
        setMode(0.0f);
        setDriveDB(0.0f);
    }

    void reset()
    {
        s1 = s2 = 0.0f;
    }

    void setSampleRate(float sr)
    {
        fs = std::max(1.0f, sr);
    }

    void setCutoff(float hz)
    {
        cutoff = std::clamp(hz, 20.0f, 0.45f * fs);
        updateCoeffs();
    }

    void setResonance(float q)
    {
        resonance = std::clamp(q, 0.05f, 20.0f);
        updateCoeffs();
    }

    // 0 = LP → 0.5 = BP → 1 = HP
    void setMode(float m)
    {
        mode = std::clamp(m, 0.0f, 1.0f);
        updateMix();
    }

    void setDriveDB(float dB)
    {
        drive = std::pow(10.0f, dB / 20.0f);
        makeup = 1.0f / std::max(drive, 1.0f);
    }

    float process(float x)
    {
        // ZDF SVF core
        const float a  = 1.0f / (1.0f + g * (g + k));
        const float v1 = a * (s1 + g * (x - s2));
        const float v2 = s2 + g * v1;

        // Saturated feedback
        float fb = k * v1 * drive;
        fb = fb / (1.0f + std::fabs(fb));

        const float hp = x - fb - v2;
        const float bp = v1;
        const float lp = v2;

        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        float y = mixHP * hp + mixBP * bp + mixLP * lp;
        return y * makeup;
    }

private:
    float fs;
    float cutoff = 1000.0f;
    float resonance = 0.707f;

    float g = 0.0f;
    float k = 0.0f;

    float s1 = 0.0f;
    float s2 = 0.0f;

    float mode = 0.0f;
    float mixLP = 1.0f;
    float mixBP = 0.0f;
    float mixHP = 0.0f;

    float drive = 1.0f;
    float makeup = 1.0f;

    void updateCoeffs()
    {
        g = std::tan(float(M_PI) * cutoff / fs);
        k = 1.0f / resonance;
    }

    void updateMix()
    {
        if (mode < 0.5f)
        {
            float t = mode * 2.0f;
            mixLP = std::cos(0.5f * float(M_PI) * t);
            mixBP = std::sin(0.5f * float(M_PI) * t);
            mixHP = 0.0f;
        }
        else
        {
            float t = (mode - 0.5f) * 2.0f;
            mixLP = 0.0f;
            mixBP = std::cos(0.5f * float(M_PI) * t);
            mixHP = std::sin(0.5f * float(M_PI) * t);
        }
    }
};
