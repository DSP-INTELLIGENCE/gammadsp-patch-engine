#pragma once

#include "Biquad3.hpp"
#include "Gamma/FormantData.h"
#include <algorithm>

class FormantMorphFilter {
public:
    using Voice   = gam::Vowel::Voice;
    using Phoneme = gam::Vowel::Phoneme;

    FormantMorphFilter()
    {
        setCurrentVowel(Voice::MAN, Phoneme::HOD);
        setTargetVowel (Voice::MAN, Phoneme::HOD);
        setResonance(8.0f);
        setMorphSpeed(0.002f);
    }

    void setCurrentVowel(Voice v, Phoneme p)
    {
        for(int i = 0; i < 3; ++i) {
            mCurrent.f[i] = gam::Vowel::freq(v, p, i);
            mCurrent.a[i] = gam::Vowel::amp (v, p, i);
        }
    }

    void setTargetVowel(Voice v, Phoneme p)
    {
        for(int i = 0; i < 3; ++i) {
            mTarget.f[i] = gam::Vowel::freq(v, p, i);
            mTarget.a[i] = gam::Vowel::amp (v, p, i);
        }
    }

    void setMorphSpeed(float speed)
    {
        mMorphSpeed = std::clamp(speed, 0.00001f, 0.1f);
    }

    void setResonance(float q)
    {
        mFilter.setRes(q);
    }

    float process(float x)
    {
        updateMorph();
        return mFilter.process(x);
    }

    void processBlock(const float* in, float* out, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    struct VowelState {
        float f[3]{};
        float a[3]{};
    };

    void updateMorph()
    {
        for(int i = 0; i < 3; ++i) {
            mCurrent.f[i] += (mTarget.f[i] - mCurrent.f[i]) * mMorphSpeed;
            mCurrent.a[i] += (mTarget.a[i] - mCurrent.a[i]) * mMorphSpeed;
        }

        mFilter.setFreqs(mCurrent.f[0], mCurrent.f[1], mCurrent.f[2]);

        float level = (mCurrent.a[0] + mCurrent.a[1] + mCurrent.a[2]) / 3.0f;
        mFilter.setLevel(level);
    }

    Biquad3 mFilter;
    VowelState mCurrent {};
    VowelState mTarget  {};
    float mMorphSpeed {};
};
