class DFXFlanger {
public:
    DFXFlanger()
    : mComb(0.01f, 0.001f)  // max 10ms, start 1ms
    {
        mComb.setIpolType(LINEAR);
        mComb.setFeedback(0.6f);
    }

    void setRate(float hz)      { mRate = hz; }
    void setDepth(float ms)    { mDepth = ms * 0.001f; }
    void setBaseDelay(float ms){ mBase = ms * 0.001f; }
    void setFeedback(float fb) { mComb.setFeedback(fb); }
    void setMix(float mix)     { mMix = std::clamp(mix, 0.f, 1.f); }

    float process(float x)
    {
        float lfo = sinf(mPhase);
        mPhase += 2.f * M_PI * mRate / gam::sampleRate();
        if (mPhase > 2.f * M_PI) mPhase -= 2.f * M_PI;

        float d = mBase + mDepth * lfo;
        mComb.setDelay(d);

        float wet = mComb.process(x);
        return x * (1.f - mMix) + wet * mMix;
    }

private:
    float mPhase = 0.f;
    float mRate  = 0.2f;    // Hz
    float mDepth = 0.002f;  // 2 ms
    float mBase  = 0.001f;  // 1 ms
    float mMix   = 0.5f;

    Comb mComb;
};

