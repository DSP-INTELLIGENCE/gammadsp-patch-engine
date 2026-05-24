class FeedbackSpectralShifter : public Function {
public:
    FeedbackSpectralShifter(float shift = 30.f)
    : shifter(shift)
    {
        setFeedback(0.6f);
        setDrive(1.f);
    }

    void setShift(float hz) { baseShift = hz; }
    void setFeedback(float f) { feedback = std::clamp(f, 0.f, 0.98f); }
    void setDrive(float d) { drive = d; }

    void reset()
    {
        shifter.reset();
        last = 0.f;
    }

    float process(float x) override
    {
        float in = x + last * feedback;

        // spectral displacement
        shifter.setShift(baseShift);

        float y = shifter.process(in * drive);

        // nonlinear stabilizer
        y = std::tanh(y);

        last = y;
        return y;
    }

private:
    FrequencyShifter shifter;
    float baseShift = 30.f;
    float feedback = 0.6f;
    float drive = 1.f;
    float last = 0.f;
};
