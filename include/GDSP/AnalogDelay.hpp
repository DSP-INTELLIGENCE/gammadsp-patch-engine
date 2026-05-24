#pragma once
#include "DigitalDelay.hpp"
#include "OnePole.hpp"
#include "SoftClipper.hpp"

class AnalogDelay : public ModDelay
{
public:
    AnalogDelay(float maxDelay = 2.0f, float initDelay = 0.35f)
    : ModDelay(maxDelay, initDelay),
      mLPF(12000.0f, gam::LOW_PASS)
    {
        // Lock all modulators into neutral state
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.0f);
        setIM(1.0f);        
        setAM(1.0f);

        setIpolType(gam::ipl::CUBIC);

        setDelay(initDelay);
        setFeedback(0.45f);
        setMix(0.30f);
        setTone(0.5f);
        setDrive(1.0f);
        setAsym(0.0f);
        setToneMod(0.0f);
        setDriveMod(0.0f);
        setAsymMod(0.0f);
    }

    // ---------- Analog character ----------

    void setTone(float t)   { mTone  = std::clamp(t, 0.0f, 1.0f); }
    void setDrive(float d)  { mDrive = std::max(0.0f, d); }
    void setAsym(float a)   { mAsym  = std::clamp(a, -1.0f, 1.0f); }

    float getTone()  const { return mTone; }
    float getDrive() const { return mDrive; }
    float getAsym()  const { return mAsym; }

    void setToneMod(float t)   { m_tone.set(t); }
    void setDriveMod(float d)  { m_drive.set(d); }
    void setAsymMod(float a)   { m_asym.set(a); }


    // ---------- DSP ----------

    float process(float input)
    {
        float delayT   = dm.process(mBaseDelay);
        float feedback = fbm.process(mBaseFeedback);
        float mix      = mixm.process(mBaseMix);
        float drive    = m_drive.process(mDrive);
        float tone     = m_tone.process(mTone);
        float asym     = m_asym.process(mAsym);

        delayT   = std::clamp(delayT, 0.0001f,_delay.getMaxDelayTime());
        feedback = std::clamp(feedback, -0.999f, 0.999f);
        mix      = std::clamp(mix, 0.0f, 1.0f);

        float in = input;
        
        _delay.setDelay(delayT);

        // ---- Read delayed signal ----
        float delayed = _delay.read();

        // ---- Analog feedback shaping ----
        float fb = delayed;
        

        // Tone: progressive HF loss
        float cutoff = 14000.0f - tone * 12000.0f;   // 14k → 2k
        mLPF.setFreq(cutoff);
        fb = mLPF.process(fb);

        // Configure clipper
        mClip.amount = 0.5f * drive;     // halve drive in loop
        mClip.knee   = 1.2f + 1.5f * drive;
        mClip.asym   = asym;

        // Saturate feedback
        fb = mClip.process(fb);

        // ---- Write back ----
        _delay.write(im.process(in) + fb * feedback);

        // ---- Mix ----
        float out = (1.0f - mix) * in  + mix * delayed;
        return am.process(out);
    }

    Modulator m_tone;
    Modulator m_drive;
    Modulator m_asym;

    float mTone  = 0.5f;
    float mDrive = 1.0f;
    float mAsym  = 0.0f;

    OnePole     mLPF;
    SoftClipper mClip;
};
