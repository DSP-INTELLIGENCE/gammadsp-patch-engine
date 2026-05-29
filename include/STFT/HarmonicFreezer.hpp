class HarmonicFreezer : public Function {
public:
    HarmonicFreezer(int bands = 16)
    {
        for(int i=0;i<bands;i++)
        {
            float f = 100.f * std::pow(2.f, i/3.f);
            filters.emplace_back(f, 5.0f, gam::BAND_PASS);
            env.emplace_back();
            phase.emplace_back(f);
            amps.push_back(0.f);
        }
    }

    void freeze(bool f) { frozen = f; }

    float process(float x) override
    {
        float out = 0.f;

        for(size_t i=0;i<filters.size();i++)
        {
            float band = filters[i].process(x);
            float e = env[i].process(std::fabs(band));

            if(!frozen)
                amps[i] = e;

            out += std::sin(phase[i].process()) * amps[i];
        }

        return out;
    }

private:
    bool frozen = false;

    std::vector<Biquad> filters;
    std::vector<EnvelopeFollower> env;
    std::vector<AccumPhase> phase;
    std::vector<float> amps;
};
