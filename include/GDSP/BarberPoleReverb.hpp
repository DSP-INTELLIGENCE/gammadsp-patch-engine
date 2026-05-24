class BarberPoleReverb : public Function {
public:
    BarberPoleReverb(int diffStages = 8, int cloudSize = 6)
    : mainShift(0.f)
    {
        for(int i=0;i<diffStages;i++)
            diffuser.emplace_back(500.f + i * 200.f);

        for(int i=0;i<cloudSize;i++)
        {
            FrequencyShifter fs;
            fs.setShift((i - cloudSize/2) * 2.f);
            cloud.push_back(fs);
        }
    }

    void setDrift(float hzPerSec) { drift = hzPerSec; }

    float process(float x) override
    {
        float y = x;
        for(auto& d : diffuser)
            y = d.process(y);

        float z = 0.f;
        for(auto& s : cloud)
            z += s.process(y);

        z /= cloud.size();

        mainShift.setShift(drift);
        z = mainShift.process(z);

        feedback = feedback * 0.995f + z;
        return feedback;
    }

private:
    std::vector<AllPass1> diffuser;
    std::vector<FrequencyShifter> cloud;
    FrequencyShifter mainShift;

    float drift = 0.2f;
    float feedback = 0.f;
};
