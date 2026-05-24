class DimensionalPhaser : public Function {
public:
    DimensionalPhaser()
    {
        for(int i=0;i<8;i++)
            phaseField.emplace_back(600.f + i * 220.f);
    }

    void setRate(float r) { rate = r; }
    void setDepth(float d) { depth = d; }

    float process(float x) override
    {
        float m = 0.5f + 0.5f * std::sin(phase);
        phase += 2.f * M_PI * rate / gam::sampleRate();

        float y = x;

        for(auto& a : phaseField)
        {
            float f = 300.f + m * 3000.f * depth;
            a.setFreq(f);
            y = a.process(y);
        }

        shifter.setShift((m - 0.5f) * 40.f * depth);
        y = shifter.process(y);

        fb = 0.8f * fb + y;
        return fb;
    }

private:
    std::vector<AllPass1> phaseField;
    FrequencyShifter shifter;

    float rate = 0.15f;
    float depth = 1.0f;
    float phase = 0.f;
    float fb = 0.f;
};
