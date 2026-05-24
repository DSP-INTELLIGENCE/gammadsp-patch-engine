class DFXBBDThroughZeroStereoFlanger : public Function
{
public:
    DFXBBDThroughZeroStereoFlanger()
    {
        setRate(0.25f);
        setDepth(0.85f);
        setFeedback(0.7f);
        setWidth(0.85f);
        setMix(0.5f);

        // Voicing
        mL.setBBDDrive(1.3f);
        mR.setBBDDrive(1.3f);
        mL.setBBDNoise(0.0016f);
        mR.setBBDNoise(0.0016f);

        // Critical: keep internal delay tiny
        mBaseDelay = 0.0012f;   // ~1.2 ms
        mL.setDelay(mBaseDelay);
        mR.setDelay(mBaseDelay * 1.01f);
    }

    void setRate(float n)
    {
        mRate = clamp01(n);
        float r = expMap(mRate, 0.03f, 1.2f);
        mL.setLFO(r);
        mR.setLFO(r * 1.03f);
    }

    void setDepth(float n)
    {
        mDepth = clamp01(n);
    }

    void setFeedback(float f)
    {
        float fb = std::clamp(f, -0.98f, 0.98f);
        mL.setFeedback(fb);
        mR.setFeedback(fb * 0.97f);
    }

    void setWidth(float w) { mWidth = clamp01(w); }

    void setMix(float m)
    {
        mMix = clamp01(m);
        mL.setWet(mMix);
        mL.setDry(1.0f - mMix);
        mR.setWet(mMix);
        mR.setDry(1.0f - mMix);
    }

    float process(float x) override
    {
        // Generate LFO
        float p = std::sinf(mPhase);
        float pR = std::sinf(mPhase + float(M_PI_2));

        mPhase += 2.f * float(M_PI) * mRate / gam::sampleRate();
        if (mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        // Through-zero delay swing
        float swing = mDepth * 0.0015f;  // up to ±1.5 ms
        float dL = mBaseDelay + swing * p;
        float dR = mBaseDelay + swing * pR;

        // Detect crossing zero
        float signL = (dL >= 0.f) ? 1.f : -1.f;
        float signR = (dR >= 0.f) ? 1.f : -1.f;

        dL = std::fabs(dL);
        dR = std::fabs(dR);

        dL = std::clamp(dL, 0.00001f, 0.008f);
        dR = std::clamp(dR, 0.00001f, 0.008f);

        mL.setDelay(dL);
        mR.setDelay(dR);

        // Process both channels
        float yL = mL.process(x) * signL;
        float yR = mR.process(x) * signR;

        // Stereo width
        float mid  = 0.5f * (yL + yR);
        float side = 0.5f * (yL - yR) * mWidth;

        return mid + side;
    }

private:
    DDXBBDDelay mL, mR;

    float mBaseDelay = 0.0012f;
    float mRate  = 0.25f;
    float mDepth = 0.85f;
    float mWidth = 0.85f;
    float mMix   = 0.5f;

    float mPhase = 0.f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }
    static float expMap(float n, float a, float b){
        return a + (b - a) * (std::exp(2.2f * n) - 1.f) / (std::exp(2.2f) - 1.f);
    }
};
