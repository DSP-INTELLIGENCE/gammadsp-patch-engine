class SpectralMorphEngine : public Function {
public:
    SpectralMorphEngine()
    {
        for(int i=0;i<8;i++)
        {
            float f = 200.f + i * 600.f;
            bands.emplace_back(f, 4.0f, gam::PEAKING);
            shifters.emplace_back(0.f);
        }
    }

    void setMorph(float m) { morph = std::clamp(m, 0.f, 1.f); }

    float process(float a, float b)
    {
        float out = 0.f;

        for(size_t i=0;i<bands.size();i++)
        {
            float sa = bands[i].process(a);
            float sb = bands[i].process(b);

            float e = sa * (1.f - morph) + sb * morph;

            shifters[i].setShift((i + 1) * 3.f * (morph - 0.5f));
            out += shifters[i].process(e);
        }

        return out / bands.size();
    }

private:
    std::vector<Biquad> bands;
    std::vector<FrequencyShifter> shifters;
    float morph = 0.0f;
};
