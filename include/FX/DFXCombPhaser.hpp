class DFXCombPhaser {
public:
    CombPhaser(int stages = 4)
    : mStages(stages)
    {
        mFilters.resize(stages);
        for (auto& f : mFilters) {
            f = std::make_unique<Comb>(0.01f, 0.001f); // up to 10ms
            f->setAllPass(0.7f);
            f->setIpolType(LINEAR);
        }
    }

    void setRate(float hz)   { mRate = hz; }
    void setDepth(float ms) { mDepth = ms * 0.001f; }
    void setBase(float ms)  { mBase = ms * 0.001f; }
    void setMix(float mix)  { mMix = std::clamp(mix, 0.f, 1.f); }

    float process(float x)
    {
        float lfo = sinf(mPhase);
        mPhase += 2.f * M_PI * mRate / gam::sampleRate();
        if (mPhase > 2.f * M_PI) mPhase -= 2.f * M_PI;

        float d = mBase + mDepth * lfo;

        float y = x;
        for (auto& f : mFilters) {
            f->setDelay(d);
            y = f->process(y);
        }

        return x * (1.f - mMix) + y * mMix;
    }

private:    
    float mPhase = 0.f;
    float mRate  = 0.4f;
    float mDepth = 0.003f; // 3 ms
    float mBase  = 0.002f; // 2 ms
    float mMix   = 0.5f;

    int mStages;
    std::vector<std::unique_ptr<Comb>> mFilters;
};
