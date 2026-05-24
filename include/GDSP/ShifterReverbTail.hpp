class ShifterReverbTail : public Function {
public:
    ShifterReverbTail(int diffStages = 8, int shifters = 6)
    {
        for(int i=0;i<diffStages;i++)
            diffuser.emplace_back(600.f + i*200.f);

        for(int i=0;i<shifters;i++)
        {
            FrequencyShifter fs;
            fs.setShift((i - shifters/2) * 2.0f);
            cloud.push_back(fs);
        }
    }

    float process(float x) override
    {
        float y = x;

        for(auto& a : diffuser)
            y = a.process(y);

        float z = 0.f;
        for(auto& s : cloud)
            z += s.process(y);

        z *= 1.f / cloud.size();

        feedback = 0.995f * feedback + 0.005f * z;
        return feedback;
    }

private:
    std::vector<AllPass1> diffuser;
    std::vector<FrequencyShifter> cloud;
    float feedback = 0.f;
};
