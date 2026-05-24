class QuantumChorus : public Function {
public:
    QuantumChorus(int voices = 8)
    {
        for(int i=0;i<voices;i++)
        {
            FrequencyShifter fs;
            fs.setShift((i - voices/2) * 0.8f);
            cloud.push_back(fs);
        }

        for(int i=0;i<6;i++)
            diffuser.emplace_back(700.f + i * 180.f);
    }

    float process(float x) override
    {
        float y = x;
        for(auto& d : diffuser)
            y = d.process(y);

        float z = 0.f;
        for(auto& s : cloud)
            z += s.process(y);

        return z / cloud.size();
    }

private:
    std::vector<AllPass1> diffuser;
    std::vector<FrequencyShifter> cloud;
};
