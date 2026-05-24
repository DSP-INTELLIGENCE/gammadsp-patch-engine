class DFXBBDBarberPoleFlanger : public Function
{
public:
    DFXBBDBarberPoleFlanger()
    {
        setRate(0.18f);
        setDepth(0.85f);
        setFeedback(0.65f);
        setWidth(0.9f);
        setMix(0.5f);
        setDirection(1.0f);

        baseDelay = 0.0012f;  // ~1.2 ms core delay

        for(int i=0;i<4;i++)
        {
            v[i].setDelay(baseDelay);
            v[i].setFeedback(0.0f);
            v[i].setBBDDrive(1.25f);
            v[i].setBBDNoise(0.0014f);
        }
    }

    // -------- controls --------
    void setRate(float n)      { rate = mapExp(n, 0.05f, 0.8f); }
    void setDepth(float n)     { depth = clamp01(n); }
    void setFeedback(float f)  { feedback = std::clamp(f, -0.95f, 0.95f); }
    void setWidth(float w)     { width = clamp01(w); }
    void setMix(float m)       { mix = clamp01(m); }
    void setDirection(float d){ direction = (d >= 0.f ? 1.f : -1.f); }

    float process(float x) override
    {
        float sr = gam::sampleRate();
        phase += direction * rate / sr;
        phase -= std::floor(phase); // wrap 0..1

        const float offsets[4] = {0.0f, 0.25f, 0.5f, 0.75f};

        float y[4];

        for(int i=0;i<4;i++)
        {
            float p = phase + offsets[i];
            p -= std::floor(p);

            // triangle wave for smooth continuous sweep
            float tri = 1.f - 4.f * std::fabs(p - 0.5f);

            // through-zero sweep
            float sweep = tri * depth * 0.0016f;
            float d = baseDelay + sweep;

            float sign = (d >= 0.f ? 1.f : -1.f);
            d = std::fabs(d);
            d = std::clamp(d, 0.00002f, 0.008f);

            v[i].setDelay(d);
            float s = v[i].process(x);
            y[i] = s * sign;
        }

        // Hadamard-style stereo projection
        float L =  0.5f*( y[0] + y[1] - y[2] - y[3] );
        float R =  0.5f*( y[0] - y[1] + y[2] - y[3] );

        // width
        float mid  = 0.5f*(L+R);
        float side = 0.5f*(L-R)*width;

        // feedback injection
        fbState = softsat_tanh(fbState * 0.98f + mid * feedback, 0.4f);

        // equal-power mix
        float t = mix * float(M_PI_2);
        float dry = std::cos(t);
        float wet = std::sin(t);

        return x * dry + (mid + side + fbState) * wet;
    }

private:
    DDXBBDDelay v[4];

    float baseDelay = 0.0012f;
    float phase = 0.f;

    float rate = 0.18f;
    float depth = 0.85f;
    float feedback = 0.65f;
    float width = 0.9f;
    float mix = 0.5f;
    float direction = 1.f;

    float fbState = 0.f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }

    static float mapExp(float n, float a, float b)
    {
        n = clamp01(n);
        float k = (std::exp(2.2f*n)-1.f)/(std::exp(2.2f)-1.f);
        return a + (b-a)*k;
    }
};
