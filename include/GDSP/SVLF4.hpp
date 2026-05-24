class SVFLadder4Filter : public ModFilter
{
public:
    SVFLadder4Filter()
    {
        baseCutoff = 1000.0f;
        baseRes    = 0.2f;     // interpret as kHat in [0..1)
        setAM(1.0f);

        mFilter.setSampleRate(44100.0f);
        mFilter.setCutoff(baseCutoff);
        mFilter.setFeedback(baseRes);
    }

    // ---- pass-through configuration (optional) ----
    void setSampleRate(float sr)                 { mFilter.setSampleRate(sr); }
    void setMode(SVFLadder4::LadderMode mode)    { mFilter.setLadderMode(mode); }
    void setType(SVFLadder4::LadderType type)    { mFilter.setType(type); }
    void setDamping(float r)                     { mFilter.setDamping(r); }           // ladder damping
    void setDrive(float d)                       { mFilter.setDrive(d); }
    void setAnalogFeel(float a)                  { mFilter.setAnalogFeel(a); }
    void setCutoffSaturation(float a)            { mFilter.setCutoffSaturation(a); }
    void enableGainComp(bool b)                  { mFilter.enableGainCompensation(b); }
    void enableAutoTrim(bool b)                  { mFilter.enableAutoTrim(b); }
    void enablePitchStabilizer(bool b)           { mFilter.enablePitchStabilizer(b); }

    // MIDI tracking pass-through (optional)
    void setBaseNote(float n)                    { mFilter.setBaseNote(n); }
    void setNote(float n)                        { mFilter.setNote(n); }
    void setKeyTracking(float t)                 { mFilter.setKeyTracking(t); }

    void reset()                                 { mFilter.reset(); }

    // ---- base parameters for ModFilter modulation ----
    void setFreq(float f) { baseCutoff = f; }
    void setRes (float r) { baseRes    = r; }    // baseRes interpreted as kHat

    // ---- Adapter layer expected by ModFilter::update() ----
    void freq(float fc) { mFilter.setCutoff(fc); }

    void res(float kHat)
    {
        // kHat must be [0..1). Clamp hard for safety.
        kHat = std::clamp(kHat, 0.0f, 0.9999f);
        mFilter.setFeedback(kHat);
    }

    float operator()(float x)
    {
        return mFilter.process(x); // mono / ch0
    }

    float process(float input) override
    {
        // Uses ModFilter's modulation law:
        // fc = baseCutoff * (1 + mCut), kHat = baseRes * (1 + mRes)
        return update(*this, input);
    }

private:
    SVFLadder4 mFilter;
};
