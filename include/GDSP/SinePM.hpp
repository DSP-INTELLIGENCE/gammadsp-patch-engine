class SinePM : public Generator {
public:

    SinePM(float f = 440.f)
    {
        setFreq(f);
        setPM(0.0f);
    }

    void setFreq(float f)       { freq = std::max(0.f, f); }
    void setPeriod(float sec)   { setFreq(1.f / std::max(1e-6f, sec)); }
    void setPhase(float p)      { phase = std::clamp((double)p, -1.0, 1.0); }
    void setPM(float v)         { pm.set(v); }    
    void setAM(float v)         { am.set(v); }
    float process() override
    {
        double pmv = pm.process();               // normalized phase offset

        // advance base phase
        phase += (freq / gam::sampleRate()) * 2.0;

        if (phase >= 1.0) phase -= 2.0;
        if (phase < -1.0) phase += 2.0;

        // apply PM in normalized domain
        double p = phase + pmv;
        if (p >= 1.0) p -= 2.0;
        if (p < -1.0) p += 2.0;

        double y = sin(M_PI * p);
        return (float)y * am.process();
    }

    Modulator pm;
    Modulator am;

private:
    double freq = 440.0;
    double phase = -1.0;
};
