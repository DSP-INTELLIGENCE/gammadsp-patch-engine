class FormantDriftProcessor : public Function {
public:
    FormantDriftProcessor()
    {
        for(int i=0;i<5;i++)
        {
            float f = 300.f + i * 700.f;
            filters.emplace_back(f, 4.0f, gam::PEAKING);
            shifters.emplace_back(0.f);
        }
    }

    void setDrift(float d) { drift = d; }

    float process(float x) override
    {
        float out = 0.f;

        for(size_t i=0;i<filters.size();i++)
        {
            float y = filters[i].process(x);

            shifters[i].setShift((i + 1) * drift);
            y = shifters[i].process(y);

            out += y;
        }

        return out / filters.size();
    }

private:
    std::vector<Biquad> filters;
    std::vector<FrequencyShifter> shifters;
    float drift = 2.0f;
};
