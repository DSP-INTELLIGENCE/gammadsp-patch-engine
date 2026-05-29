class TimeCollapseEngine : public Function {
public:
    TimeCollapseEngine()
    : gravity(-0.4f), drift(0.15f)
    {
        for(int i=0;i<8;i++)
            diffuser.emplace_back(500.f + i * 200.f);

        freezer = HarmonicFreezer(16);
    }

    float process(float x) override
    {
        float y = x;

        for(auto& d : diffuser)
            y = d.process(y);

        y = freezer.process(y);

        y = gravity.process(y);

        y = drift.process(y);

        feedback = feedback * 0.998f + y;

        return feedback;
    }

private:
    std::vector<AllPass1> diffuser;
    HarmonicFreezer freezer;
    FrequencyShifter gravity;
    FrequencyShifter drift;

    float feedback = 0.f;
};
