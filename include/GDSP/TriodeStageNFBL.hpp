struct TriodeStageNFBL
{
    float sr = 44100.f;

    Triode triode;

    TPT1Pole inputHP;
    TPT1Pole plateLP;

    NonlinearFeedbackLoop fbLoop;

    float bias = 0.f;
    float biasAmount = 0.4f;
    float drive = 2.0f;

    void init(float fs)
    {
        sr = fs;
        inputHP.setHPCut(20.f, sr);
        plateLP.setLPCut(8000.f, sr);
        reset();
    }

    void reset()
    {
        inputHP.reset();
        plateLP.reset();
        bias = 0.f;
    }

    inline float process(float x)
    {
        // 1. Input coupling capacitor
        x = inputHP.processHP(x);

        // 2. Envelope-based bias drift (tube memory)
        float env = fabsf(x);
        float targetBias = -biasAmount * env;
        bias += 0.01f * (targetBias - bias);

        // 3. Get feedback from plate
        float plateFeedback = plateLP.s;

        // 4. Shape feedback nonlinearly
        float fb = fbLoop.process(plateFeedback, x);

        // 5. Grid-to-cathode voltage
        float vgk = x * drive + bias - fb;

        // 6. Plate voltage estimate (never zero)
        float vpk = std::max(0.1f, plateLP.s);

        // 7. Triode current
        float ip = triode.process(vgk, vpk);

        // 8. Plate load (RC lowpass)
        float y = plateLP.processLP(-ip);

        return y;
    }
};
