class SpectralDiffusionChamber : public Function {
public:
    SpectralDiffusionChamber(int diffStages = 12, int shifters = 10)
    {
        for(int i=0;i<diffStages;i++)
            diff.emplace_back(400.f + i * 150.f);

        for(int i=0;i<shifters;i++)
        {
            FrequencyShifter fs;
            fs.setShift((i - shifters/2) * 1.5f);
            cloud.push_back(fs);
        }
    }

    float process(float x) override
    {
        float y = x;

        for(auto& d : diff)
            y = d.process(y);

        float z = 0.f;
        for(auto& s : cloud)
            z += s.process(y);

        z /= cloud.size();

        feedback = feedback * 0.996f + z;

        return feedback;
    }

private:
    std::vector<AllPass1> diff;
    std::vector<FrequencyShifter> cloud;
    float feedback = 0.f;
};
