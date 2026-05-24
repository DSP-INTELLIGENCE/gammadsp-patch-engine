struct DynamicGraphicEQBand
{
    Biquad eq;
    Biquad detector;
    EnvFollow env;

    float baseGainDB = 0.0f;
    float threshold  = 0.1f;
    float ratio      = 2.0f;

    void setup(float freq, float q)
    {
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

class DynamicGraphicEQ
{
public:
    static constexpr int NumBands = 10;

    void setup(const float* freqs, float q)
    {
        for(int i = 0; i < NumBands; ++i)
            bands[i].setup(freqs[i], q);
    }

    inline float process(float x)
    {
        for(auto& b : bands)
            x = b.process(x);   // 🔥 serial processing, not summing
        return x;
    }

private:
    DynamicGraphicEQBand bands[NumBands];
};
