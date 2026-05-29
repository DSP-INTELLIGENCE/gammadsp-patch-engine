class EventHorizonReverb : public Function {
public:
    EventHorizonReverb(int diffStages = 10, int shifters = 6)
    : gravity(0.f)
    {
        for(int i=0;i<diffStages;i++)
            diffuser.emplace_back(600.f + i * 180.f);

        for(int i=0;i<shifters;i++)
        {
            FrequencyShifter fs;
            fs.setShift(-(i + 1) * 1.5f);
            cloud.push_back(fs);
        }
    }

    float process(float x) override
    {
        float y = x;

        for(auto& d : diffuser)
            y = d.process(y);

        float z = 0.f;
        for(auto& s : cloud)
            z += s.process(y);

        z /= cloud.size();

        z = gravity.process(z);

        feedback = feedback * 0.997f + z;
        return feedback;
    }

private:
    std::vector<AllPass1> diffuser;
    std::vector<FrequencyShifter> cloud;
    FrequencyShifter gravity;
    float feedback = 0.f;
};
