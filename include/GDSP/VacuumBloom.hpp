class VacuumBloom : public Function {
public:
    VacuumBloom()
    {
        for(int i=0;i<6;i++)
            diff.emplace_back(800.f + i * 240.f);
    }

    float process(float x) override
    {
        float y = x;

        for(auto& a : diff)
            y = a.process(y);

        float m = 0.5f + 0.5f * std::sin(phase);
        phase += 2.f * M_PI * 0.08f / gam::sampleRate();

        bloom.setShift(m * 20.f);
        y = bloom.process(y);

        return y;
    }

private:
    std::vector<AllPass1> diff;
    FrequencyShifter bloom;
    float phase = 0.f;
};
