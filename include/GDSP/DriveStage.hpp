struct DriveStage
{
    // Controls
    float drive = 1.f;          // 0.5 → clean, 4+ → aggressive
    float bias  = 0.f;          // asymmetry / tube bias
    float trim  = 1.f;          // output trim

    // Internal smoothing
    float driveLP = 1.f;
    float biasLP  = 0.f;

    // Denormal safety
    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    // Core soft saturation
    inline float softSat(float x){
        return x / (1.f + fabsf(x));
    }

    // Loudness compensation
    inline float loudComp() const {
        return 1.f / (1.f + 0.4f * drive);
    }

    inline float process(float x)
    {
        // smooth controls
        driveLP += 0.002f * (drive - driveLP);
        biasLP  += 0.002f * (bias  - biasLP);

        // pre-gain
        float y = x * driveLP;

        // asymmetry (even harmonics)
        y += biasLP * y * y;

        // soft saturation (analog headroom)
        y = softSat(y);

        // output trim & loudness balance
        y *= trim * loudComp();

        return zap(y);
    }
};
