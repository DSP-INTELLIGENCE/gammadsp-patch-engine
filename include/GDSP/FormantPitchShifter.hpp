class FormantPitchShifter : public Function {
public:
    FormantPitchShifter()
    : shifter(0.f)
    {}

    void setRatio(float r) { ratio = std::clamp(r, 0.25f, 4.f); }
    void setFormantAmount(float a) { formantAmt = std::clamp(a, 0.f, 1.f); }

    float process(float x) override
    {
        float y = pitch.process(x, ratio);

        // crude but effective spectral centroid proxy
        float env = envelope.process(std::fabs(y));
        float formantShift = (ratio - 1.f) * env * 400.f;

        shifter.setShift(-formantShift * formantAmt);

        return shifter.process(y);
    }

private:
    PitchShifter pitch;
    FrequencyShifter shifter;

    float ratio = 1.f;
    float formantAmt = 1.f;

    EnvelopeFollower envelope;
};
