struct DeviceModel
{
    // ===== Device character =====
    float knee        = 1.0f;   // softness of transition
    float symmetry    = 1.0f;   // + side gain
    float asymmetry   = 1.0f;   // - side gain
    float compression = 0.0f;   // envelope-dependent softening
    float biasSense   = 0.0f;   // how strongly bias affects curve
    float hfDamping   = 0.0f;   // suppress HF at high drive

    // ===== Internal state =====
    float driveLP = 1.0f;   // dynamic knee modulation

    // ===== Core transfer (canonical S-curve) =====
    inline float transfer(float x)
    {
        // asymmetry
        if (x >= 0.f) x *= symmetry;
        else          x *= asymmetry;

        // algebraic soft clip (stable, cheap, good harmonics)
        float y = x / std::sqrt(1.f + knee * x * x);

        return y;
    }

    // ===== Main process =====
    inline float process(float x, float env, float bias)
    {
        // bias affects operating point
        x += bias * biasSense;

        // envelope softens knee (tube-like compression)
        float k = knee * (1.f + compression * env);

        // asymmetry already applied in transfer
        if (x >= 0.f) x *= symmetry;
        else          x *= asymmetry;

        float y = x / std::sqrt(1.f + k * x * x);

        return y;
    }
};

struct NonLinearStageDevice
{
    DeviceModel d;

    enum Type {
        TUBE,
        TRANSISTOR,
        DIODE
    };

    TPT1Pole biasLP;
    TPT1Pole sagLP;

    float baseBias   = 0.f;
    float biasAmount = 0.f;
    float sagAmount  = 0.f;

    float bias = 0.f;

    NonLinearStage(Type t = TUBE) {
        setType(t);
    }
    void setType(Type t) {
        switch(t) {
            case TUBE: makeTube(); break;
            case DIODE:  makeDiode(); break;
            case TRANSISTOR: makeTransistor(); break;
            default: makeTube(); break;
        }        
    }
    float process(float x, float env, float drive)
    {
        
        float targetBias = baseBias - biasAmount * env;
        bias = biasLP.processLP(targetBias);

        float v = x * drive;

        float y = d.process(v, env, bias);

        float sagTarget = 1.f / (1.f + sagAmount * env);
        float sag = sagLP.processLP(sagTarget);

        return y * sag;
    }

    void reset()
    {
        biasLP.reset();
        sagLP.reset();
        bias = 0.f;
    }

    void makeTube()
    {        
        d.knee        = 2.0f;
        d.symmetry    = 1.15f;
        d.asymmetry   = 0.85f;
        d.compression = 0.8f;
        d.biasSense   = 1.0f;
        d.hfDamping   = 0.4f;
        
    }

    void makeDiode()
    {     
        d.knee        = 6.0f;
        d.symmetry    = 1.0f;
        d.asymmetry   = 1.0f;
        d.compression = 0.2f;
        d.biasSense   = 0.3f;
        d.hfDamping   = 0.0f;
        
    }

    void makeTransistor()
    {     
        d.knee        = 10.0f;
        d.symmetry    = 1.3f;
        d.asymmetry   = 0.6f;
        d.compression = 0.5f;
        d.biasSense   = 1.5f;
        d.hfDamping   = 0.2f;
        
    }
};
