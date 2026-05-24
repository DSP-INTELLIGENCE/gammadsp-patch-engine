#include "IntelligentVocoder.hpp"

static float hzToQ(float f)
{
    return std::max(0.5f, f * 0.001f);
}

IntelligentVocoder::IntelligentVocoder(float sr, int bands)
: mSampleRate(sr)
{
    mBands.resize(bands);

    float fMin = 80.0f;
    float fMax = 8000.0f;

    for (int i = 0; i < bands; ++i)
    {
        float t = (float)i / (float)(bands - 1);
        float freq = fMin * std::pow(fMax / fMin, t);

        float Q = hzToQ(freq);

        mBands[i].analysis.setBandpass(freq, Q, sr);
        mBands[i].synth.setBandpass(freq, Q, sr);
    }
}

void IntelligentVocoder::reset()
{
    for (auto& b : mBands)
    {
        b.analysis.reset();
        b.synth.reset();
        b.env.value = 0.0f;
    }
}

float IntelligentVocoder::process(float modulator, float carrier,
                                  const AudioFeatures& F,
                                  const MusicalState& M,
                                  const ExpressionState& E)
{
    float out = 0.0f;

    for (auto& band : mBands)
    {
        float m = band.analysis.process(modulator);

        float drive = std::abs(m);

        // musical shaping
        drive *= (0.5f + F.energy * 0.6f);
        if (F.onset) drive *= 1.2f;
        drive *= (0.7f + E.intensity * 0.6f);

        float env = band.env.process(drive, mSampleRate);

        float c = band.synth.process(carrier);
        out += c * env;
    }

    out *= 0.5f + E.intensity * 0.7f;

    return out * mDryWet + carrier * (1.0f - mDryWet);
}
