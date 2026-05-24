class DopplerField : public Function {
public:
    DopplerField()
    {
        motionRate = 0.1f;
        distance = 0.5f;
    }

    void setSpeed(float s)    { speed = s; }
    void setDistance(float d) { distance = std::clamp(d, 0.f, 1.f); }

    float process(float x) override
    {
        float m = 0.5f + 0.5f * std::sin(phase);
        phase += 2.f * M_PI * motionRate / gam::sampleRate();

        float v = (m - 0.5f) * speed;

        // Doppler: frequency shift from velocity
        shifter.setShift(v * 80.f);

        // Propagation delay from distance
        modFM.setDepth(distance * 0.02f);

        float y = modFM.process(x);
        y = shifter.process(y);

        for(auto& a : air)
            y = a.process(y);

        return y;
    }

private:
    float motionRate;
    float speed = 1.f;
    float distance;

    float phase = 0.f;

    ModFM modFM;
    FrequencyShifter shifter;
    std::vector<AllPass1> air;
};

