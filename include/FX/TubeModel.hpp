struct TubeModel
{
    // Controls
    float drive = 1.f;     // how hard the tube is driven
    float bias  = 0.f;     // grid bias / asymmetry
    float plate = 1.f;     // plate supply (headroom feel)
    float trim  = 1.f;

    // Smoothing
    float drvLP = 1.f;
    float biasLP = 0.f;
    float plateLP = 1.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core triode curve (approximation)
    inline float triode(float x)
    {
        // soft knee compression
        float y = x / (1.f + fabsf(x));

        // plate starvation behavior
        y *= plateLP / (1.f + 0.5f * fabsf(y));

        // even harmonics from bias
        y += biasLP * y * y;

        return y;
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.45f * drvLP);
    }

    inline float process(float x)
    {
        // smooth parameters
        drvLP   += 0.002f * (drive - drvLP);
        biasLP  += 0.002f * (bias  - biasLP);
        plateLP += 0.002f * (plate - plateLP);

        // drive input
        float y = x * drvLP;

        // tube shaping
        y = triode(y);

        // output trim
        y *= trim * loudComp();

        return zap(y);
    }
};
