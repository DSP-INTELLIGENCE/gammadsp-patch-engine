class DFXCombResonator : public Function {
public:
    DFXCombResonator()
    {
        comb.setFeedback(0.995f);
        comb.setMix(1.0f);
    }

    void setFrequency(float hz)
    {
        if (hz > 1.f)
            comb.setDelay(1.f / hz);
    }

    void setDecay(float d)
    {
        comb.setFeedback(std::clamp(d, 0.90f, 0.99999f));
    }

    float process(float x) override
    {
        return comb.process(x);
    }

private:
    Comb comb;
};

