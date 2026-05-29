// TransistorFuzzCircuit.hpp
// assumes: DistortionCircuit, TPT1Pole, Waveshaper, AnalogStage exist

struct TransistorFuzzCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;
    AnalogStage stage;

    TPT1Pole preHP;     // input coupling / bass trim
    TPT1Pole preLP;     // bandwidth limit into fuzz
    TPT1Pole postLP;    // tone shaping

    // user controls
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed controls
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // DC blocker
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // ----------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, shaper);

        // Fuzz-style defaults
        stage.couplingHP.setHPCut(20.f, sr);
        stage.envLP.setLPCut(25.f, sr);
        stage.storageLP.setLPCut(1.5f, sr);

        stage.baseBias   = 0.1f;    // transistor idle bias
        stage.biasAmount = 0.15f;   // very strong bias shift
        stage.sagAmount  = 0.25f;   // extreme supply sag

        preHP.setHPCut(70.f, sr);
        preLP.setLPCut(4500.f, sr);
        postLP.setLPCut(3500.f, sr);

        // Aggressive nonlinearity
        shaper.setMode(Waveshaper::WS_HARDCLIP);
        shaper.setDrive(1.f);
        shaper.setCurve(1.f);
        shaper.setOutGain(1.f);
        shaper.setAM(1.f);
    }

    void reset() override
    {
        stage.reset();
        preHP.reset();
        preLP.reset();
        postLP.reset();

        driveLP = drive01;
        toneLP  = tone01;
        levelLP = level01;

        dc_x1 = dc_y1 = 0.f;
    }

    // ----------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Transistor Fuzz"; }

    // ----------------------------------------------------

    inline float zap(float x) const
    {
        return (std::fabs(x) < 1e-20f) ? 0.f : x;
    }

    inline float dcBlock(float x)
    {
        float y = x - dc_x1 + 0.995f * dc_y1;
        dc_x1 = x; dc_y1 = y;
        return y;
    }

    // ----------------------------------------------------

    float process(float x) override
    {
        // smooth knobs
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // ---------------------------
        // Control mapping
        // ---------------------------

        // Drive: 0..+60 dB (fuzz is extreme)
        float dB = 60.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone: moves both preLP & postLP (classic fuzz tone feel)
        float preHz  = 3000.f * std::pow(10.f, std::log10(9000.f / 3000.f) * toneLP);
        float postHz = 1200.f * std::pow(10.f, std::log10(6000.f / 1200.f) * toneLP);
        preLP.setLPCut(preHz, sr);
        postLP.setLPCut(postHz, sr);

        // Level: -36..+6 dB
        float leveldB = -36.f + 42.f * levelLP;
        float outGain = std::pow(10.f, leveldB / 20.f);

        // ---------------------------
        // Circuit flow
        // ---------------------------

        x = stage.couplingHP.processHP(x);
        x = preHP.processHP(x);
        x = preLP.processLP(x);

        float env = stage.envLP.processLP(std::fabs(x));

        float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        float sag = 1.f / (1.f + stage.sagAmount * env);

        float y = shaper.process((x * stage.drive + stage.bias) * sag);

        y = postLP.processLP(y);

        return dcBlock(zap(y * outGain));
    }
};
