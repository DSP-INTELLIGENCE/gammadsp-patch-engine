class InharmonicFMShifter : public Function {
public:
    InharmonicFMShifter(float baseShift = 50.f)
    : shifter(baseShift)
    {
        setFeedback(0.6f);
        setFMDepth(100.f);
    }

    void setBaseShift(float hz) { base = hz; }
    void setFMDepth(float hz)   { fmDepth = hz; }
    void setFeedback(float f)   { fb = std::clamp(f, 0.f, 0.98f); }

    void reset()
    {
        shifter.reset();
        last = 0.f;
    }

    float process(float x) override
    {
        // feedback injection
        float in = x + last * fb;

        // generate inharmonic modulation from previous output
        float fm = last * fmDepth;

        shifter.setShift(base + fm);

        float y = shifter.process(in);

        // gentle safety
        y = std::tanh(y);

        last = y;
        return y;
    }

private:
    FrequencyShifter shifter;

    float base = 50.f;
    float fmDepth = 100.f;
    float fb = 0.6f;
    float last = 0.f;
};
