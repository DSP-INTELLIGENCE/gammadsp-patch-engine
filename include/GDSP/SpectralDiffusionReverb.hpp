class SpectralDiffusionReverb : public Function {
public:
    SpectralDiffusionReverb(int diffStages = 10, int shifters = 8)
    {
        for(int i=0;i<diffStages;i++)
            diffuser.emplace_back(500.f + i * 180.f);

        for(int i=0;i<shifters;i++)
        {
            FrequencyShifter fs;
            fs.setShift((i - shifters/2) * 3.f);
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

        z /= cloud.size();

        feedback = feedback * 0.995f + z;
        return feedback;
    }

private:
    std::vector<AllPass1> diffuser;
    std::vector<FrequencyShifter> cloud;
    float feedback = 0.f;
};
