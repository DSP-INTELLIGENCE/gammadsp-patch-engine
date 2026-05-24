class ModSpatializer : public Function
{
public:
    ModSpatializer()
    {
        setPosition(0.f, 0.f, 2.f);
        setRoom(0.4f);
        setWidth(1.0f);
        setMix(1.0f);
    }

    void setPosition(float azimuth, float elevation, float distance)
    {
        az = azimuth;
        el = elevation;
        dist = std::max(0.1f, distance);
    }

    void setRoom(float r)  { room = std::clamp(r, 0.f, 1.f); }
    void setWidth(float w) { width = std::clamp(w, 0.f, 1.f); }
    void setMix(float m)   { mix = std::clamp(m, 0.f, 1.f); }

    std::pair<float,float> process(float input)
    {
        // Distance attenuation
        float gain = 1.f / (1.f + dist);

        // Air absorption
        float cutoff = 18000.f / (1.f + dist);
        airLPF.setFreq(cutoff);

        float x = airLPF.process(input * gain);

        // Stereo pan from azimuth
        auto lr = pan(x, az, width);

        // Room
        float roomSig = reverb.process(x) * room;

        float L = lr.first  + roomSig;
        float R = lr.second + roomSig;

        return { L * mix, R * mix };
    }

private:
    static std::pair<float,float> pan(float s, float az, float width)
    {
        float p = std::sin(az) * width;
        float a = (p + 1.f) * 0.5f;
        float L = s * std::cos(a * 1.5707963f);
        float R = s * std::sin(a * 1.5707963f);
        return { L, R };
    }

    float az = 0.f, el = 0.f, dist = 2.f;
    float room = 0.4f, width = 1.f, mix = 1.f;

    OnePole airLPF;
    ModReverb reverb;
};
