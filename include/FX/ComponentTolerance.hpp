struct ComponentTolerance
{
    // permanent per-voice offsets
    float cutoffMul = 1.f;
    float resMul    = 1.f;
    float driveMul  = 1.f;
    float biasMul   = 1.f;

    float gmMul     = 1.f;
    float envMul    = 1.f;
    float sagMul    = 1.f;

    // random generator
    static inline float rnd(float a, float b){
        return a + (b - a) * (float(rand()) / float(RAND_MAX));
    }

    void reset()
    {
        // typical analog tolerances: 1–5%
        cutoffMul = rnd(0.985f, 1.015f);
        resMul    = rnd(0.97f,  1.03f);
        driveMul  = rnd(0.95f,  1.05f);
        biasMul   = rnd(0.90f,  1.10f);

        gmMul     = rnd(0.95f,  1.05f);
        envMul    = rnd(0.95f,  1.05f);
        sagMul    = rnd(0.95f,  1.05f);
    }
};
