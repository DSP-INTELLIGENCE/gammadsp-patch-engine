
// Uses your TPT1Pole
struct AnalogStage
{
    float sr = 44100.f;

    // DSP blocks
    TPT1Pole couplingHP;   // blocking / DC removal
    TPT1Pole storageLP;    // bias memory / sag
    TPT1Pole envLP;        // envelope follower

    Waveshaper* shaper = nullptr;

    // parameters
    float drive      = 1.0f;
    float baseBias   = 0.0f;
    float biasAmount = 0.35f;  // how much envelope moves bias
    float sagAmount  = 0.25f;

    // state
    float bias = 0.f;

    // ---------- setup ----------
    void init(float sampleRate, Waveshaper* ws){
        sr = sampleRate;
        shaper = ws;

        couplingHP.setHPCut(10.f, sr);   // typical coupling cap
        storageLP.setLPCut(5.f, sr);     // bias memory
        envLP.setLPCut(15.f, sr);        // envelope smoothing
    }

    void reset(){
        couplingHP.reset();
        storageLP.reset();
        envLP.reset();
        bias = 0.f;
    }

    // ---------- main process ----------
    float process(float x)
    {
        // 1) DC blocking / coupling cap
        x = couplingHP.processHP(x);

        // 2) Envelope of *input* (what the device actually sees)
        float env = envLP.processLP(std::fabs(x));

        // 3) Bias memory (slow control system)
        float targetBias = baseBias - biasAmount * env;
        bias = storageLP.processLP(targetBias);

        // 4) Drive + bias into device
        float v = x * drive + bias;

        // 5) Safety clamp (not tone shaping)
        //v = std::clamp(v, -8.f, 8.f);

        // 6) Nonlinear device law
        float y = shaper->process(v);

        // 7) Power-supply sag / compression
        float sag = 1.f / (1.f + sagAmount * env);

        return y * sag;
    }
};

class DistortionCircuit {
public:
    virtual void prepare(float sampleRate) = 0;
    virtual float process(float x) = 0;
    virtual void reset() = 0;

    virtual void setDrive(float v01) = 0;
    virtual void setTone(float v01) = 0;
    virtual void setLevel(float v01) = 0;

    virtual std::string name() const = 0;
};

class OverdriveCircuit : public DistortionCircuit {
protected:
    AnalogStage stage;
    Waveshaper shaper;
    SimpleTone tone;
    GainStage out;
};

