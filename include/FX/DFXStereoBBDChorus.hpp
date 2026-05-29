class StereoBBDChorus : public Function
{
public:
    StereoBBDChorus()
    {
        setRate(0.35f);
        setDepth(0.45f);
        setDelay(0.020f);
        setWidth(0.75f);
        setMix(0.55f);

        // voicing
        mL.setBBDDrive(1.1f);
        mR.setBBDDrive(1.1f);
        mL.setBBDNoise(0.0012f);
        mR.setBBDNoise(0.0012f);
    }

    void setRate(float n)
    {
        float r = expMap(clamp01(n), 0.05f, 3.0f);
        mL.setLFO(r);
        mR.setLFO(r * 1.03f);
    }

    void setDepth(float n)
    {
        float d = 0.0015f + clamp01(n) * 0.004f;
        mL.setDepth(d);
        mR.setDepth(d * 1.05f);
    }

    void setDelay(float sec)
    {
        mL.setDelay(sec);
        mR.setDelay(sec * 1.01f);
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

        // width matrix
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * mWidth;

        return mid + side;   // mono return; route L/R externally if engine supports it
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
    float mWidth = 0.75f;
    float mMix = 0.55f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }
    static float expMap(float n, float a, float b){
        return a + (b - a) * (std::exp(2.5f * n) - 1.f) / (std::exp(2.5f) - 1.f);
    }
};
