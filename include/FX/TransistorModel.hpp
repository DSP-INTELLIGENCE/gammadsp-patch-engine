struct TransistorModel
{
    // Controls
    float drive = 1.f;     // gain into transistor
    float bias  = 0.f;     // operating point shift
    float headroom = 1.f;  // supply voltage feel
    float trim  = 1.f;

    // Smoothing
    float drvLP = 1.f;
    float biasLP = 0.f;
    float headLP = 1.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core transistor curve
    inline float transistor(float x)
    {
        // base-emitter knee
        float y = std::tanh(2.2f * x);

        // headroom limiting
        y = y / (1.f + 0.7f * fabsf(y) / headLP);

        // bias asymmetry
        y += biasLP * y * y;

        return y;
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.4f * drvLP);
    }

    inline float process(float x)
    {
        // smooth parameters
        drvLP  += 0.002f * (drive    - drvLP);
        biasLP += 0.002f * (bias     - biasLP);
        headLP += 0.002f * (headroom - headLP);

        // drive
        float y = x * drvLP;

        // nonlinear shaping
        y = transistor(y);

        // output
        y *= trim * loudComp();

        return zap(y);
    }
};
