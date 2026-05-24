
// DFXDigitalDelay.hpp
#pragma once

/////////////////////////////////////////////////////////////////
// DigitalDelay
/////////////////////////////////////////////////////////////////
struct DFXDelayPreset
{
    float delay  = 0.25f;
    float fbk    = 0.35f;
    float mix    = 0.5f;
    float depth  = 0.0f;
    float wet    = 1.0f;
    float dry    = 1.0f;
};

/*
DelayPreset ChorusPreset {
    0.020f,   // delay
    0.0f,     // feedback
    0.6f,     // mix
    0.25f,    // depth
    1.0f,     // wet
    1.0f      // dry
};

DelayPreset FlangerPreset {
    0.002f,
    0.7f,
    0.5f,
    0.8f,
    1.0f,
    1.0f
};

DelayPreset VibratoPreset {
    0.005f,
    0.0f,
    1.0f,
    0.6f,
    1.0f,
    0.0f
};
*/

class DFXDigitalDelay : public Function
{
public:
    DFXDigitalDelay(float maxDelaySeconds = 2.0f,
                 float initDelaySeconds = 0.25f)
    : mDelay(maxDelaySeconds, initDelaySeconds)
    {}

    

    void setPreset(const DFXDelayPreset& p)
    {
        setDelay(p.delay);
        setFeedback(p.fbk);
        setMix(p.mix);
        setDepth(p.depth);
        setWet(p.wet);
        setDry(p.dry);
    }

    void setDelay(float seconds)    { baseDelay = seconds; }
    void setFeedback(float fb)      { baseFeedback = std::clamp(fb, -0.9995f, 0.9995f); }
    void setMix(float mix)          { baseMix = std::clamp(mix, 0.0f, 1.0f); }
    
    void setWet(float wet)          { mWet = std::max(0.0f, wet); }
    void setDry(float dry)          { mDry = std::max(0.0f, dry); }
    
    float getDelay()   const { return baseDelay; }
    float getFeedback()const { return baseFeedback; }
    float getMix()     const { return baseMix; }
    float getDelayTime() const { return baseDelay; }

    void setDelayMod(float v)       { dm.set(v); }
    void setFbkMod(float v)         { fm.set(v); }
    void setMixMod(float v)         { mm.set(v); }
    void setAM(float v)             { am.set(v); }
    void setDepth(float v)          { depth.set(v); }
    void setLFO(float m)            { lfo.set(m); }

float getCurrentDelay() const
    {
        return (mCurrentDelay > 0.f) ? mCurrentDelay : baseDelay;
    }

    float process(float x) override
    {
        float delayT = lfo.process(depth.process(dm.process(baseDelay)));

        float feedback = fm.process(baseFeedback);
        float mix      = mm.process(baseMix);

        delayT   = std::max(delayT, 0.0001f);
        feedback = std::clamp(feedback, -0.9995f, 0.9995f);
        mix      = std::clamp(mix, 0.0f, 1.0f);

        // ✅ capture actual delay
        mCurrentDelay = delayT;

        if (delayT != mLastDelay)
        {
            mDelay.delay(delayT);
            mLastDelay = delayT;
        }

        float d = mDelay.read();
        mDelay.write(x + d * feedback);

        float wet = d * mWet;
        float dry = x * mDry;
        float y   = dry * (1.0f - mix) + wet * mix;

        return am.process(y);
    }


    Modulator dm, fm, mm, am, lfo, depth;

private:
    gam::Delay<double> mDelay;

    float baseDelay    = 0.25f;
    float baseFeedback = 0.35f;
    float baseMix      = 0.5f;

    float mWet = 1.0f;
    float mDry = 1.0f;

    float mLastDelay = -1.0f;

    // ✅ new
    float mCurrentDelay = 0.0f;
};
