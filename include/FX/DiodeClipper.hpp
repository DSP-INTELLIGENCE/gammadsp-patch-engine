struct DiodeClipper
{
    // Controls
    float drive = 1.f;       // input gain
    float threshold = 0.6f;  // diode knee voltage
    float asym = 0.f;        // asymmetry (even harmonics)
    float trim = 1.f;

    // Smoothing
    float drvLP = 1.f;
    float thrLP = 0.6f;
    float asymLP = 0.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Diode transfer curve
    inline float diode(float x)
    {
        float a = thrLP;

        // smooth diode knee
        float y = x;
        if (x >  a) y = a + (x - a) / (1.f + (x - a)*(x - a));
        if (x < -a) y = -a + (x + a) / (1.f + (x + a)*(x + a));

        // asymmetry / rectifier flavor
        y += asymLP * y * y;

        return y;
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.35f * drvLP);
    }

    inline float process(float x)
    {
        // smooth parameters
        drvLP  += 0.002f * (drive     - drvLP);
        thrLP  += 0.002f * (threshold - thrLP);
        asymLP += 0.002f * (asym      - asymLP);

        // drive
        float y = x * drvLP;

        // nonlinear clipping
        y = diode(y);

        // output trim
        y *= trim * loudComp();

        return zap(y);
    }
};
