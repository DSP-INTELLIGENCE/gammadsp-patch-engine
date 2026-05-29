/////////////////////////////////////////////////////////////////
// DigitalDelay
/////////////////////////////////////////////////////////////////
struct SmoothDelayPreset
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

class DFXSmoothDigitalDelay : public Function
{
public:
    DFXSmoothDigitalDelay(float maxDelaySeconds = 2.0f,
                 float initDelaySeconds = 0.25f)
    : mDelay(maxDelaySeconds, initDelaySeconds)
    {
        delaySmooth.setType(Smoother::P_ONEPOLE);
        feedbackSmooth.setType(Smoother::P_SLEW);
        mixSmooth.setType(Smoother::P_LINEAR);

        delaySmooth.setTime(6.0f);     // ms — critical for chorus/flanger quality
        feedbackSmooth.setTime(20.0f);
        mixSmooth.setTime(10.0f);

        delaySmooth.set(initDelaySeconds);
        feedbackSmooth.set(baseFeedback);
        mixSmooth.set(baseMix);
    }

    void setPreset(const DelayPreset& p)
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

    void setDelayMod(float v)       { dm.set(v); }
    void setFbkMod(float v)         { fm.set(v); }
    void setMixMod(float v)         { mm.set(v); }
    void setAM(float v)             { am.set(v); }
    void setDepth(float v)          { depth.set(v); }
    void setLFO(float v)            { lfo.set(v); }

    float getBaseDelay() const { return baseDelay; }
    float getCurrentDelay() const { return mCurrentDelay; } // smoothed delay used by mDelay


    float process(float x) override
    {
        float dm1 = dm.process();
        float dm2 = lfo.process() * depth.process();

        float delayTarget = baseDelay * std::exp2(dm1 + dm2);

        float feedbackTarget = baseFeedback * (1.0f + fm.process());
        float mixTarget      = baseMix      * (1.0f + mm.process());

        delayTarget     = std::max(delayTarget, 0.0001f);
        feedbackTarget  = std::clamp(feedbackTarget, -0.9995f, 0.9995f);
        mixTarget       = std::clamp(mixTarget, 0.0f, 1.0f);

        delaySmooth.set(delayTarget);
        feedbackSmooth.set(feedbackTarget);
        mixSmooth.set(mixTarget);

        float delayT   = delaySmooth.process();
        float feedback = feedbackSmooth.process();
        float mix      = mixSmooth.process();

        mCurrentDelay = delayT; // ✅ store the true running delay time

        if (std::abs(delayT - mLastDelay) > 1e-9f)
        {
            mDelay.delay(delayT);
            mLastDelay = delayT;
        }

        float d = mDelay.read();
        mDelay.write(x + d * feedback);

        float wet = d * mWet;
        float dry = x * mDry;
        float a   = mix * float(M_PI_2);
        float y   = dry * std::cos(a) + wet * std::sin(a);

        return y * am.process();
    }
    // Modulators
    Modulator dm, fm, mm, am, lfo, depth;

private:
    gam::Delay<double> mDelay;

    float baseDelay    = 0.25f;
    float baseFeedback = 0.35f;
    float baseMix      = 0.5f;

    float mWet = 1.0f;
    float mDry = 1.0f;

    float mLastDelay = -1.0f;

    // Smoothers
    Smoother delaySmooth;
    Smoother feedbackSmooth;
    Smoother mixSmooth;
};

