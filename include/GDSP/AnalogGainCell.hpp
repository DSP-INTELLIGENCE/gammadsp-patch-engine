struct AnalogGainCell : public Function
{
    // --- conditioning ---
    TPT1Pole couplingHP;

    // --- memory ---
    TPT1Pole envLP;
    TPT1Pole biasLP;

    // --- parameters ---
    float drive      = 1.0f;
    float baseBias   = 0.0f;
    float biasAmount = 0.3f;
    float sagAmount  = 0.2f;

    // --- state ---
    float bias = 0.0f;

    void init(float sr)
    {
        couplingHP.setHPCut(10.f, sr);
        envLP.setLPCut(20.f, sr);
        biasLP.setLPCut(2.f, sr);
        reset();
    }

    void reset()
    {
        couplingHP.reset();
        envLP.reset();
        biasLP.reset();
        bias = 0.f;
    }

    float process(float x) override
    {
        // coupling cap
        x = couplingHP.processHP(x);

        // envelope (rectified output, not input)
        float env = envLP.processLP(std::fabs(x));

        // bias drift
        float targetBias = baseBias - biasAmount * env;
        bias = biasLP.processLP(targetBias);

        // nonlinear core
        float v = x * drive + bias;
        float y = std::tanh(v);

        // supply sag (acts like slow compression)
        float sag = 1.f / (1.f + sagAmount * env);
        return y * sag;
    }
};
