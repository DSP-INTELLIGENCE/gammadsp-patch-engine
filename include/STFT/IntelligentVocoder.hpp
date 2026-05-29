#pragma once
#include <vector>
#include <cmath>
#include "Biquad.hpp"
#include "AnalysisBus.hpp"

struct BandEnvelope {
    float value = 0.0f;
    float attack = 0.01f;
    float release = 0.08f;

    float process(float x, float sr)
    {
        float coeffA = std::exp(-1.0f / (attack * sr));
        float coeffR = std::exp(-1.0f / (release * sr));

        if (x > value) value = coeffA * value + (1 - coeffA) * x;
        else           value = coeffR * value + (1 - coeffR) * x;

        return value;
    }
};

struct VocoderBand {
    Biquad analysis;
    Biquad synth;
    BandEnvelope env;
};

class IntelligentVocoder {
public:
    IntelligentVocoder(float sr, int bands = 16);

    void reset();

    float process(float modulator, float carrier,
                  const AudioFeatures& F,
                  const MusicalState&  M,
                  const ExpressionState& E);

    void setDryWet(float v) { mDryWet = std::clamp(v, 0.0f, 1.0f); }

private:
    float mSampleRate;
    std::vector<VocoderBand> mBands;
    float mDryWet = 0.5f;
};
