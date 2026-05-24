class ModSidechainPump : public Function
{
public:
    ModSidechainPump()
    {
        setAttack(0.005f);
        setRelease(0.25f);
        setDepth(0.7f);
        setMix(1.0f);
    }

    void setAttack(float a)  { attack = std::clamp(a, 0.001f, 0.1f); }
    void setRelease(float r) { release = std::clamp(r, 0.01f, 2.0f); }
    void setDepth(float d)   { depth = std::clamp(d, 0.f, 1.f); }
    void setMix(float m)     { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x) override
    {
        // envelope follower
        float inputLevel = std::fabs(x);
        float coeff = inputLevel > env ? attack : release;
        env += (inputLevel - env) * coeff;

        // gain curve (ducking)
        float gain = 1.f - depth * env;

        // smoothing
        smoothedGain += (gain - smoothedGain) * 0.01f;

        float y = x * smoothedGain;
        return x * (1.f - mix) + y * mix;
    }

private:
    float attack  = 0.005f;
    float release = 0.25f;
    float depth   = 0.7f;
    float mix     = 1.0f;

    float env = 0.f;
    float smoothedGain = 1.f;
};
