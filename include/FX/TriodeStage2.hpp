struct TriodeStage
{
    float sr = 44100.f;

    // --- Core tube device ---
    Triode triode;

    // --- Analog conditioning blocks ---
    TPT1Pole inputHP;     // input coupling capacitor
    TPT1Pole biasLP;      // slow bias memory
    TPT1Pole plateLP;     // plate RC load (voltage state)

    // --- State ---
    float bias = 0.f;

    // --- User controls ---
    float drive      = 2.5f;   // input gain
    float biasAmount = 0.4f;   // envelope → bias depth

    // --- Setup ---
    void init(float fs)
    {
        sr = fs;

        inputHP.setHPCut(20.f, sr);     // typical coupling capacitor
        biasLP.setLPCut(5.f, sr);       // very slow bias memory
        plateLP.setLPCut(8000.f, sr);   // plate load bandwidth

        reset();
    }

    void reset()
    {
        inputHP.reset();
        biasLP.reset();
        plateLP.reset();
        bias = 0.f;
    }

    inline float process(float x)
    {
        // Input conditioning
        x = inputHP.processHP(x);

        // Envelope → slow bias migration
        float env = std::fabs(x);
        float targetBias = -biasAmount * env;
        bias = biasLP.processLP(targetBias);

        // Grid voltage
        float vgk = x * drive + bias;

        // Continuous grid conduction softening
        float gpos = std::max(0.f, vgk);
        vgk -= 0.5f * gpos;

        // Plate voltage from previous state
        float vpk = std::max(0.1f, plateLP.s);

        // Triode current
        float ip = triode.process(vgk, vpk);
        ip = std::clamp(ip, -10.f, 10.f);   // numeric safety

        // Plate RC load integration
        float y = plateLP.processLP(-ip);

        // Keep plate voltage physically valid
        plateLP.s = std::max(0.1f, plateLP.s);

        return y;
    }

};
