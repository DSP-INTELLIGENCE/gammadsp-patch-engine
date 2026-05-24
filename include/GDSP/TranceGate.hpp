class ModTranceGate : public Function
{
public:
    static constexpr int MAX_STEPS = 32;

    ModTranceGate()
    {
        setBPM(120.f);
        setSteps(16);
        setPattern({1,0,1,0, 1,0,1,0, 1,1,0,0, 1,0,1,0});
        setSmooth(0.02f);
        setDepth(1.0f);
        setMix(1.0f);
    }

    void setBPM(float bpm) { this->bpm = std::max(30.f, bpm); }
    void setSteps(int s)   { steps = std::clamp(s, 1, MAX_STEPS); }

    void setPattern(const std::vector<int>& p)
    {
        for (int i = 0; i < steps; ++i)
            pattern[i] = (i < (int)p.size()) ? (p[i] != 0) : 1;
    }

    void setSmooth(float s) { smooth = std::clamp(s, 0.f, 0.1f); }
    void setDepth(float d)  { depth = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)    { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        // step timing
        float stepTime = 60.f / bpm / 4.f;  // 16th notes
        counter += 1.f / (float)gam::sampleRate();

        if (counter >= stepTime)
        {
            counter -= stepTime;
            stepIndex = (stepIndex + 1) % steps;
        }

        float gate = pattern[stepIndex] ? 1.f : 0.f;

        // envelope smoothing
        float target = 1.f - depth + depth * gate;
        env += (target - env) * (smooth * 1000.f);

        float y = x * env;
        return x * (1.f - mix) + y * mix;
    }

private:
    float bpm = 120.f;

    int steps = 16;
    int stepIndex = 0;
    bool pattern[MAX_STEPS]{};

    float counter = 0.f;

    float smooth = 0.02f;
    float depth = 1.0f;
    float mix = 1.0f;

    float env = 0.f;
};
