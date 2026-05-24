class ShifterVocoder {
public:
    ShifterVocoder(int bands = 8)
    {
        bands = std::clamp(bands, 4, 16);

        for(int i = 0; i < bands; ++i)
        {
            float f = 200.f * std::pow(2.f, i / 2.f);
            analysis.emplace_back(f, 0.7f, gam::BAND_PASS);
            envelopes.emplace_back();
            shifters.emplace_back(0.f);
        }
    }

    float process(float modulator, float carrier)
    {
        float out = 0.f;

        for(size_t i = 0; i < shifters.size(); ++i)
        {
            float band = analysis[i].process(modulator);
            float env  = envelopes[i].process(std::fabs(band));

            shifters[i].setShift(env * 200.f);

            out += shifters[i].process(carrier);
        }

        return out / shifters.size();
    }

private:
    std::vector<Biquad> analysis;
    std::vector<EnvelopeFollower> envelopes;
    std::vector<FrequencyShifter> shifters;
};
