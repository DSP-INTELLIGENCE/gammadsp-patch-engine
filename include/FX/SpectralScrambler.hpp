class SpectralScrambler : public Function {
public:
    SpectralScrambler(int bands = 6)
    {
        bands = std::clamp(bands, 2, 12);

        for (int i = 0; i < bands; ++i) {
            float f = 120.f * std::pow(2.f, i);
            filters.emplace_back(f, 0.7f, gam::BAND_PASS);
            shifters.emplace_back((i - bands/2) * 15.f);
            gains.push_back(1.f / bands);
        }
    }

    void randomize()
    {
        for (size_t i = 0; i < shifters.size(); ++i)
            shifters[i].setShift(randomRange(-300.f, 300.f));
    }

    float process(float x) override
    {
        float out = 0.f;

        for (size_t i = 0; i < filters.size(); ++i)
        {
            float b = filters[i].process(x);
            b = shifters[i].process(b);
            out += b * gains[i];
        }

        return out;
    }

private:
    float randomRange(float a, float b) {
        return a + (b - a) * (rand() / (float)RAND_MAX);
    }

    std::vector<Biquad> filters;
    std::vector<FrequencyShifter> shifters;
    std::vector<float> gains;
};
