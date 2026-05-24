class RBJTVSVFFilter : public ModFilter
{
public:
    ModRBJFilter()
    {
        baseCutoff = 1000.0f;
        baseRes    = 0.707f;
        setAM(1.0f);

        mFilter.setCutoff(baseCutoff);
        mFilter.setQ(baseRes);
    }

    // Filter type & shape control
    void setType(RBJType t)           { mFilter.setType(t); }
    void setAlphaMode(RBJAlphaMode m) { mFilter.setAlphaMode(m); }
    void setGainDB(float db)          { mFilter.setGainDB(db); }
    void setBandwidth(float bw)       { mFilter.setBandwidth(bw); }
    void setShelfSlope(float s)       { mFilter.setShelfSlope(s); }

    // Base params (no modulation)
    void setFreq(float f) { baseCutoff = f; mFilter.setCutoff(f); }
    void setRes (float r) { baseRes    = r; mFilter.setQ(r); }

    void setSmoothing(float a) { mFilter.setSmoothing(a); }

    void reset() { mFilter.reset(); }

    float process(float input) override
    {
        return update(mFilter, input);
    }

private:
    RBJTVSVF mFilter;
};
