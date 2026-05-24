struct AsymmetryController
{
    // -1 .. +1
    float amount = 0.f;
    float amountLP = 0.f;

    // bias memory
    float bias = 0.f;

    inline float zap(float x)
    {
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    float process(float x)
    {
        // smooth control
        amountLP += 0.002f * (amount - amountLP);

        // dynamic bias
        bias += 0.01f * (amountLP - bias);

        // apply asymmetry
        float y = x + bias * 0.3f;

        // asymmetric saturation
        if (y >= 0.f)
            y = y / (1.f + fabsf(y));
        else
            y = y / (1.f + 0.5f * fabsf(y));

        return zap(y);
    }
};
