#pragma once
#include "ModDelay.hpp"
#include "OnePole.hpp"
#include "Allpass1Block.hpp"
#include "PitchShifter.hpp"

class ShimmerDelay : public ModDelay
{
public:
    ShimmerDelay(float maxDelay = 8.0f, float initDelay = 0.6f)
    : ModDelay(maxDelay, initDelay)
    {
        setDelay(initDelay);
        setFeedback(0.55f);
        setMix(0.35f);

        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);

        setShimmerAmount(1.0f);
        setTone(0.5f);
        setDiffusion(0.6f);
    }

    // ---- Shimmer controls ----

    void setShimmerAmount(float a) { mShimmer = std::clamp(a, 0.f, 1.f); }
    void setTone(float t)          { mTone = std::clamp(t, 0.f, 1.f); }
    void setDiffusion(float d)     { mDiffusion = std::clamp(d, 0.f, 1.f); }

    float process(float input) override
    {
        float delayT   = dm.process(mBaseDelay);
        float feedback = fbm.process(mBaseFeedback);
        float mix      = mixm.process(mBaseMix);

        delayT   = std::clamp(delayT, 0.01f, _delay.getMaxDelayTime());
        feedback = std::clamp(feedback, 0.f, 0.99f);
        mix      = std::clamp(mix, 0.f, 1.f);

        _delay.setDelay(delayT);

        float d = _delay.read();

        // ---- Shimmer feedback path ----
        float fb = d;

        // Pitch shift inside feedback
        float shifted = mPitch.process(fb);  // +12 semitones

        // Diffusion network (smearing the shimmer)
        float diff = shifted;
        diff = diffusers.process(diff);

        // Tone shaping
        float cutoff = 12000.f - mTone * 9000.f;
        mLPF.setFreq(cutoff);
        diff = mLPF.process(diff);

        // Blend original feedback with shimmer
        fb = (1.0f - mShimmer) * fb + mShimmer * diff;

        _delay.write(im.process(input) + fb * feedback);

        float out = (1.0f - mix) * input + mix * d;
        return am.process(out);
    }

private:
    PitchShifter mPitch;
    OnePole      mLPF;
    AllPass1Block diffusers;

    float mShimmer = 1.0f;
    float mTone = 0.5f;
    float mDiffusion = 0.6f;
};
