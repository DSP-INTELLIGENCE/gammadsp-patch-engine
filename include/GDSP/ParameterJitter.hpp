struct ParameterJitter
{
    // depth and speed of the micro fluctuations
    float amount = 0.0003f;   // how far the value wanders
    float speed  = 0.03f;     // how fast it moves

    float value = 0.f;

    inline float zap(float x)
    {
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float rnd()
    {
        return 2.f * (float(rand()) / float(RAND_MAX)) - 1.f;
    }

    inline float process()
    {
        // generate new micro target each sample
        float target = rnd() * amount;

        // smooth random motion
        value += speed * (target - value);

        return zap(value);
    }
};
