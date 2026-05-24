class StereoSplit
{
public:
    StereoSplit()
    {
        setWidth(0.7f);
        setTimeOffset(0.002f);  // 2 ms natural widening
        setModOffset(0.35f);    // L/R modulation phase difference
    }

    // ---------- Controls ----------

    void setWidth(float w)
    {
        mWidth = std::clamp(w, 0.f, 1.f);
    }

    void setTimeOffset(float sec)
    {
        mTimeOffset = std::clamp(sec, 0.f, 0.02f);
    }

    void setModOffset(float amt)
    {
        mModOffset = std::clamp(amt, 0.f, 1.f);
    }

    FX& left()  { return fxL; }
    FX& right() { return fxR; }

    // ---------- Audio ----------

    void process(float in, float& outL, float& outR)
    {
        // decorrelated input
        float inL = in;
        float inR = in;

        // optional micro delay widening
        if (mTimeOffset > 0.f)
        {
            mMicroDelay.write(in);
            inR = mMicroDelay.readAt(mTimeOffset);
        }

        // modulation offset trick (if FX has LFOs)
        applyModOffset();

        float l = fxL.process(inL);
        float r = fxR.process(inR);

        // stereo width matrix
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * mWidth;

        outL = mid + side;
        outR = mid - side;
    }

private:
    FX fxL, fxR;

    float mWidth      = 0.7f;
    float mTimeOffset = 0.002f;
    float mModOffset  = 0.35f;

    Delay mMicroDelay { 0.02f, 0.0f };

    void applyModOffset()
    {
        // Optional: only if FX exposes LFO phase control
        if constexpr (requires(FX f) { f.setPhaseOffset(0.f); })
        {
            fxL.setPhaseOffset(0.0f);
            fxR.setPhaseOffset(mModOffset);
        }
    }
};
