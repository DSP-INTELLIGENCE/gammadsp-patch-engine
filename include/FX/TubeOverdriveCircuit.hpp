// TubeOverdriveCircuit.hpp
// assumes: DistortionCircuit, TPT1Pole, Waveshaper, AnalogStage exist
// Notes:
// - Uses AnalogStage for coupling HP + envelope + bias memory + sag.
// - Uses Waveshaper in a “triode-ish” mode for the core nonlinearity.
// - Uses simple pre/post filtering for “tone” (bright ↔ dark).
// - Includes output DC block + parameter smoothing.

struct TubeOverdriveCircuit : public DistortionCircuit
{
    float sr = 44100.f;

    Waveshaper  shaper;   // nonlinear core (triode-ish)
    AnalogStage stage;    // couplingHP, envLP, storageLP, bias, sag, drive

    // tone shaping
    TPT1Pole preHP;   // tightens low end into the drive stage (guitar amp-ish)
    TPT1Pole preLP;   // bandwidth limiting before clipping
    TPT1Pole postLP;  // tone control smoothing after clipping

    // user knobs (0..1)
    float drive01 = 0.5f;
    float tone01  = 0.5f;
    float level01 = 0.5f;

    // smoothed knobs
    float driveLP = 0.5f;
    float toneLP  = 0.5f;
    float levelLP = 0.5f;

    // output DC block
    float dc_x1 = 0.f, dc_y1 = 0.f;

    // -------------------------------------------------

    void prepare(float sampleRate) override
    {
        sr = sampleRate;
        reset();

        stage.init(sr, shaper);

        // “tube-ish” circuit defaults
        stage.couplingHP.setHPCut(25.f, sr);   // coupling cap
        stage.envLP.setLPCut(15.f, sr);        // envelope follower
        stage.storageLP.setLPCut(2.5f, sr);    // bias memory (slow)

        stage.baseBias   = 0.f;
        stage.biasAmount = 0.06f;              // stronger than diode: tube bias shift
        stage.sagAmount  = 0.10f;              // tube supply sag feel

        // pre/post filters
        preHP.setHPCut(90.f, sr);              // keep lows from flubbing out
        preLP.setLPCut(9000.f, sr);            // tame harsh HF into clip
        postLP.setLPCut(6500.f, sr);           // default tone

        // Choose a triode-ish curve (your earlier Waveshaper has WS_TRIODE)
        shaper.setMode(Waveshaper::WS_TRIODE);
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

    // -------------------------------------------------

    void setDrive(float v01) override { drive01 = std::clamp(v01, 0.f, 1.f); }
    void setTone (float v01) override { tone01  = std::clamp(v01, 0.f, 1.f); }
    void setLevel(float v01) override { level01 = std::clamp(v01, 0.f, 1.f); }

    std::string name() const override { return "Tube Overdrive"; }

    // -------------------------------------------------

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

    // -------------------------------------------------

    float process(float x) override
    {
        // smooth knobs
        driveLP += 0.002f * (drive01 - driveLP);
        toneLP  += 0.002f * (tone01  - toneLP);
        levelLP += 0.002f * (level01 - levelLP);

        // -------------------------------------------------
        // knob mapping
        // -------------------------------------------------

        // Drive: 0..+45 dB (tube is typically a little less extreme than diode/fuzz)
        // square law gives finer control in low/mid drive
        const float dB = 45.f * (driveLP * driveLP);
        stage.drive = std::pow(10.f, dB / 20.f);

        // Tone: darker -> lower postLP cutoff, brighter -> higher cutoff
        // log sweep 1800..12000 Hz
        const float lo = 1800.f;
        const float hi = 12000.f;
        const float postHz = lo * std::pow(10.f, std::log10(hi / lo) * toneLP);
        postLP.setLPCut(postHz, sr);

        // Also modulate preLP slightly with tone (brighter = more bandwidth into clip)
        const float preLo = 5500.f;
        const float preHi = 12000.f;
        const float preHz = preLo * std::pow(10.f, std::log10(preHi / preLo) * toneLP);
        preLP.setLPCut(preHz, sr);

        // Level: -30..+6 dB (tube OD often needs more cut)
        const float leveldB = -30.f + 36.f * levelLP;
        const float outGain = std::pow(10.f, leveldB / 20.f);

        // -------------------------------------------------
        // circuit flow
        // -------------------------------------------------

        // input coupling (DC block) + low-end tightening
        x = stage.couplingHP.processHP(x);
        x = preHP.processHP(x);

        // bandwidth into the nonlinear stage
        x = preLP.processLP(x);

        // envelope follower
        const float env = stage.envLP.processLP(std::fabs(x));

        // dynamic bias shift (tube grid bias moves with signal)
        const float targetBias = stage.baseBias - stage.biasAmount * env;
        stage.bias = stage.storageLP.processLP(targetBias);
        stage.bias = zap(stage.bias);

        // sag (supply compression)
        const float sag = 1.f / (1.f + stage.sagAmount * env);

        // nonlinear core (triode-ish)
        float y = shaper.process((x * stage.drive + stage.bias) * sag);

        // post smoothing / tone
        y = postLP.processLP(y);

        // output DC safety + level
        return dcBlock(zap(y * outGain));
    }
};
