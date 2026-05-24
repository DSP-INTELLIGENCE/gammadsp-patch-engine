struct BiasInstability
{
    float amount = 0.0002f;     // drift depth
    float rate   = 0.00001f;    // how fast bias wanders

    float state = 0.f;

    inline float rnd(){
        return 2.f * (float(rand()) / float(RAND_MAX)) - 1.f;
    }

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float process()
    {
        // random walk
        state += rate * rnd();

        // gentle limiting
        state = std::clamp(state, -amount, amount);

        return zap(state);
    }
};
