class StereoBBDFlanger : public Function
{
public:
    StereoBBDFlanger()
    {
        setRate(0.30f);
        setDepth(0.65f);
        setDelay(0.002f);
        setFeedback(0.6f);
        setWidth(0.8f);
        setMix(0.5f);

        mL.setBBDDrive(1.3f);
        mR.setBBDDrive(1.3f);
        mL.setBBDNoise(0.0018f);
        mR.setBBDNoise(0.0018f);
    }

    void setRate(float n)
    {
        float r = expMap(clamp01(n), 0.05f, 1.5f);
        mL.setLFO(r);
        mR.setLFO(r * 1.04f);
    }

    void setDepth(float n)
    {
        float d = 0.0003f + clamp01(n) * 0.0012f;
        mL.setDepth(d);
        mR.setDepth(d * 1.07f);
    }

    void setDelay(float sec)
    {
        mL.setDelay(sec);
        mR.setDelay(sec * 1.02f);
    }

    void setFeedback(float f)
    {
        float fb = std::clamp(f, -0.95f, 0.95f);
        mL.setFeedback(fb);
        mR.setFeedback(fb * 0.98f);
    }

    void setWidth(float w) { mWidth = clamp01(w); }

    void setMix(float n)
    {
        mMix = clamp01(n);
        mL.setWet(mMix); mL.setDry(1.0f - mMix);
        mR.setWet(mMix); mR.setDry(1.0f - mMix);
    }

    float process(float x) override
    {
        float l = mL.process(x);
        float r = mR.process(x);

        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * mWidth;

        return mid + side;
    }
    StereoSample process(float x)
    {
        float l = mL.process(x);
        float r = mR.process(x);

        StereoSample s;
        s.outL = l;
        s.outR = r;
        return s;
    }
private:
    DDXBBDDelay mL, mR;
    float mWidth = 0.8f;
    float mMix   = 0.5f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }
    static float expMap(float n, float a, float b){
        return a + (b - a) * (std::exp(2.0f * n) - 1.f) / (std::exp(2.0f) - 1.f);
    }
};
