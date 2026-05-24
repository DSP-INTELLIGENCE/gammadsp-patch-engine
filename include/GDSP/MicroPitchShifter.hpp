class MicroPitchShifter : public Function {
public:
    MicroPitchShifter()
    {
        setDepth(6.f);   // cents
        setRate(0.15f);  // Hz
        setMix(0.5f);
    }

    void setDepth(float cents) { depth = cents; }
    void setRate(float hz)     { rate = hz; }
    void setMix(float m)       { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        phase += 2.f * M_PI * rate / gam::sampleRate();
        if (phase > 2.f * M_PI) phase -= 2.f * M_PI;

        float mod = std::sin(phase);

        float centsUp   =  depth * mod;
        float centsDown = -depth * mod;

        float ratioUp   = std::pow(2.f, centsUp   / 1200.f);
        float ratioDown = std::pow(2.f, centsDown / 1200.f);

        float u = up.process(x, ratioUp);
        float d = down.process(x, ratioDown);

        float y = (u + d) * 0.5f;

        return x * (1.f - mix) + y * mix;
    }

private:
    PitchShifter up, down;
    float depth = 6.f;
    float rate  = 0.15f;
    float phase = 0.f;
    float mix   = 0.5f;
};
