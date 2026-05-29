#pragma once

#include "Biquad3.hpp"
#include "Gamma/FormantData.h"

class FormantFilter {
public:
    using Voice   = gam::Vowel::Voice;
    using Phoneme = gam::Vowel::Phoneme;

    FormantFilter()
    {
        // reasonable default: male "ah"
        setVowel(Voice::MAN, Phoneme::HOD);
        mFilter.setRes(8.0f);
    }

    void setVowel(Voice v, Phoneme p)
    {
        mVoice = v;
        mPhoneme = p;

        float f0 = gam::Vowel::freq(v, p, 0);
        float f1 = gam::Vowel::freq(v, p, 1);
        float f2 = gam::Vowel::freq(v, p, 2);

        mFilter.setFreqs(f0, f1, f2);

        // Average amplitude of formants
        float a0 = gam::Vowel::amp(v, p, 0);
        float a1 = gam::Vowel::amp(v, p, 1);
        float a2 = gam::Vowel::amp(v, p, 2);

        float level = (a0 + a1 + a2) / 3.0f;
        mFilter.setLevel(level);
    }

    void setResonance(float q)
    {
        mFilter.setRes(q);
    }

    float process(float x)
    {
        return mFilter.process(x);
    }

    void processBlock(const float* in, float* out, size_t n)
    {
        mFilter.run(in, out, n);
    }

private:
    Biquad3 mFilter;
    Voice   mVoice;
    Phoneme mPhoneme;
};
