struct DynamicParametricEQBand
{
    Biquad eq;
    Biquad detector;
    EnvFollow env;

    float freq       = 1000.0f;
    float q          = 1.0f;
    float baseGainDB = 0.0f;

    float threshold  = 0.1f;
    float ratio      = 2.0f;

    void setup(float f, float Q, float gainDB)
    {
        freq = f;
        q    = Q;
        baseGainDB = gainDB;

        eq.setType(gam::PEAKING);
        eq.setFreq(freq);
        eq.setRes(q);

        detector.setType(gam::BAND_PASS);
        detector.setFreq(freq);
        detector.setRes(q);

        eq.setLevel(dbToLinear(baseGainDB));
    }

    inline float process(float x)
    {
        float detect = detector.process(x);
        float level  = env.process(std::abs(detect));

        float gainDB = baseGainDB;

        if (level > threshold)
        {
            float over = level - threshold;
            gainDB -= over * (1.0f - 1.0f / ratio);
        }

        eq.setLevel(dbToLinear(gainDB));
        return eq.process(x);
    }

    static inline float dbToLinear(float db)
    {
        return std::pow(10.0f, db * 0.05f);
    }
};

class DynamicParametricEQ
{
public:
    static constexpr int MaxBands = 8;

    void addBand(float freq, float q, float gainDB)
    {
        if (numBands < MaxBands)
            bands[numBands++].setup(freq, q, gainDB);
    }

    void clear() { numBands = 0; }

    inline float process(float x)
    {
        for (int i = 0; i < numBands; ++i)
            x = bands[i].process(x);
        return x;
    }

private:
    int numBands = 0;
    DynamicParametricEQBand bands[MaxBands];
};
