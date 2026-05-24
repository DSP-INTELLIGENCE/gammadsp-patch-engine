#pragma once
#include "AnalogDelay.hpp"
#include "LFO.hpp"
#include "OnePole.hpp"
#include "Noise.hpp"

class TapeDelay : public AnalogDelay
{
public:
    TapeDelay(float maxDelay = 6.0f, float initDelay = 0.45f)
    : AnalogDelay(maxDelay, initDelay),
      mToneLPF(14000.0f, gam::LOW_PASS)
    {
        setWow(0.0025f);
        setFlutter(0.0005f);
        setAge(0.35f);
        setNoise(0.002f);
    }

    // ---------- Tape controls ----------

    void setWow(float v)     { mWowAmt     = std::max(0.0f, v); }
    void setFlutter(float v) { mFlutterAmt = std::max(0.0f, v); }
    void setAge(float v)     { mAge        = std::clamp(v, 0.0f, 1.0f); }
    void setNoise(float v)   { mNoiseAmt   = std::max(0.0f, v); }

    // modulators
    void setWowMod(float v) { m_wow.set(v); }
    void setFlutterMod(float v) { m_flutter.set(v); }
    void setNoiseMod(float v) { m_noise.set(v); }
    void setAgeMod(float v) { m_age.set(v); }

    // ---------- DSP ----------

    float process(float input)
    {
        float delayT   = dm.process(mBaseDelay);
        float feedback = fbm.process(mBaseFeedback);
        float mix      = mixm.process(mBaseMix);
        float drive    = m_drive.process(mDrive);
        float tone     = m_tone.process(mTone);
        float asym     = m_asym.process(mAsym);

        float wow      = m_wow.process(mWowAmt);
        float flutter  = m_flutter.process(mFlutterAmt);
        float noise    = m_noise.process(mNoise.process() * mNoiseAmt);        
        float age      = m_age.process(mAge);
        
        delayT   = std::clamp(delayT, 0.0001f,_delay.getMaxDelayTime());
        feedback = std::clamp(feedback, -0.999f, 0.999f);
        mix      = std::clamp(mix, 0.0f, 1.0f);

        

        float in = input;

        // ---- Time instability ----
        float mod = wow + flutter;
        delayT = delayT + mod*delayT;
        float t = std::clamp(
            delayT,
            0.001f,
            _delay.getMaxDelayTime()
        );

        _delay.setDelay(t);

        // ---- Read ----
        float delayed = _delay.read();

        // ---- Tape coloration in feedback ----
        float fb = delayed;

        // Age → progressive HF loss
        float cutoff = 14000.0f - age * 12000.0f;
        mToneLPF.setFreq(cutoff);
        fb = mToneLPF.process(fb);

        // Age → tape compression & saturation
        mClip.amount = drive;
        mClip.knee   = 0.8f + 2.0f * drive;
        mClip.asym   = 0.2f * drive;
        fb = mClip.process(fb);

        // Energy loss (critical)
        float loss = 1.0f - 0.12f * age;
        fb *= std::clamp(loss, 0.7f, 1.0f);

        // Tape hiss inside loop
        fb += noise * (0.2f + age);

        // DC protection
        static float dc = 0.0f;
        dc = 0.995f * dc + 0.005f * fb;
        fb -= dc;

        // ---- Write ----
        _delay.write(im.process(in) + fbm.process(fb * mBaseFeedback));

        // ---- Mix + tape hiss ----
        float out = (1.0f - mix) * in + mix * delayed;
        out += noise * (0.2f + age);

        return am.process(out);
    }

    // Modulators
    Modulator m_wow;
    Modulator m_flutter;
    Modulator m_noise;
    Modulator m_age;

private:
    
    // Tape character
    float mWowAmt     = 0.0025f;
    float mFlutterAmt = 0.0005f;
    float mAge        = 0.35f;
    float mNoiseAmt   = 0.002f;

    OnePole mToneLPF;
    NoiseWhite   mNoise;
};
