#pragma once
#include "FormantMorphFilter.hpp"
#include "Engine.hpp"
#include "Parameters.hpp"
#include <algorithm>

class ModVocalTract : public Function
{
public:
    ModVocalTract()
    {
        setMix(1.0f);
        setFB(0.0f);
        setIM(1.0f);
        setAM(1.0f);
        setMorphSpeed(0.002f);
    }

    // ---------- Base controls ----------
    void setMix(float v)        { baseMix = std::clamp(v,0.f,1.f); }
    void setFB(float v)         { baseFB  = std::clamp(v,-0.99f,0.99f); }
    void setMorphSpeed(float s){ morphSpeed = std::clamp(s, 0.00001f, 0.1f); }

    // ---------- Modulation ----------
    void setFM(float v) { fm.set(v); }    // vowel motion
    void setAM(float v) { am.set(v); }
    void setIM(float v) { im.set(v); }

    // ---------- Vowel control ----------
    void setCurrentVowel(FormantMorphFilter::Voice v,
                          FormantMorphFilter::Phoneme p)
    { formant.setCurrentVowel(v,p); }

    void setTargetVowel(FormantMorphFilter::Voice v,
                         FormantMorphFilter::Phoneme p)
    { formant.setTargetVowel(v,p); }

    float process(float input) override
    {
        float _fm = fm.process();

        // Modulate morph speed (vowel motion)
        formant.setMorphSpeed(morphSpeed * (1.0f + 0.8f * _fm));

        float x = input * im.process() + last * baseFB;

        float y = formant.process(x);

        float out = input * (1.f - baseMix) + y * baseMix;
        out *= am.process();

        last = out;
        return out;
    }

private:
    FormantMorphFilter formant;

    float baseMix = 1.f;
    float baseFB  = 0.f;
    float morphSpeed = 0.002f;

    float last = 0.f;

    Modulator fm;
    Modulator am;
    Modulator im;
};
