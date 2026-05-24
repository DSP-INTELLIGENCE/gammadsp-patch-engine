class PitchFreeFlanger : public Function {
public:
    PitchFreeFlanger(int stages = 6)
    {
        for(int i=0;i<stages;i++)
            ap.emplace_back(800.f);
    }

    void setDepth(float d) { depth = std::clamp(d, 0.f, 1.f); }
    void setRate(float r)  { lfoFreq = r; }

    float process(float x) override
    {
        float m = 0.5f + 0.5f * std::sin(phase);
        phase += 2.f * M_PI * lfoFreq / gam::sampleRate();

        float y = x;
        for(auto& s : ap)
        {
            float f = 300.f + m * 2000.f * depth;
            s.setFreq(f);
            y = s.process(y);
        }

        return x + y;
    }

private:
    std::vector<AllPass1> ap;
    float depth = 0.7f;
    float lfoFreq = 0.2f;
    float phase = 0.f;
};
