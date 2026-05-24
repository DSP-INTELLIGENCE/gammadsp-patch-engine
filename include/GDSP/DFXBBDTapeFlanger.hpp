class DFXBBDTapeFlanger : public Function
{
public:
    DFXBBDTapeFlanger()
    {
        setRate(0.12f);
        setDepth(0.9f);
        setFeedback(0.65f);
        setWidth(0.9f);
        setMix(0.5f);

        setWow(0.18f);
        setFlutter(0.7f);
        setDrift(0.15f);

        baseDelay = 0.0011f;   // head spacing ~1.1ms

        mL.setDelay(baseDelay);
        mR.setDelay(baseDelay * 1.01f);

        mL.setBBDDrive(1.4f);
        mR.setBBDDrive(1.4f);
        mL.setBBDNoise(0.0016f);
        mR.setBBDNoise(0.0016f);
    }

    // -------- user controls --------
    void setRate(float n)     { rate = mapExp(n, 0.03f, 0.7f); }
    void setDepth(float n)    { depth = clamp01(n); }
    void setFeedback(float f){ feedback = std::clamp(f, -0.98f, 0.98f); }
    void setWidth(float w)    { width = clamp01(w); }
    void setMix(float m)      { mix = clamp01(m); }

    void setWow(float n)      { wowAmt = clamp01(n); }
    void setFlutter(float n)  { flutAmt = clamp01(n); }
    void setDrift(float n)    { driftAmt = clamp01(n); }

    float process(float x) override
    {
        float sr = gam::sampleRate();

        // base sweep
        phase += rate / sr;
        phase -= std::floor(phase);

        // primary sweep shape (tape hand pressure curve)
        float tri = 1.f - 4.f * std::fabs(phase - 0.5f);

        // wow / flutter / drift
        wowPhase += 2.f * float(M_PI) * (0.15f + 0.25f * wowAmt) / sr;
        flutPhase += 2.f * float(M_PI) * (0.8f + 3.0f * flutAmt) / sr;

        float wow   = std::sinf(wowPhase);
        float flut  = std::sinf(flutPhase) * std::sinf(0.47f * flutPhase + 1.1f);
        float drift = driftState = driftState * 0.9995f + (rand01() - 0.5f) * 0.0003f * driftAmt;

        float mod = tri + 0.25f * wow + 0.12f * flut + drift;

        // through-zero tape delay motion
        float sweep = mod * depth * 0.0016f;
        float dL = baseDelay + sweep;
        float dR = baseDelay + sweep * 0.98f;

        float signL = (dL >= 0.f ? 1.f : -1.f);
        float signR = (dR >= 0.f ? 1.f : -1.f);

        dL = std::fabs(dL);
        dR = std::fabs(dR);

        dL = std::clamp(dL, 0.00002f, 0.008f);
        dR = std::clamp(dR, 0.00002f, 0.008f);

        mL.setDelay(dL);
        mR.setDelay(dR);

        // process
        float yL = mL.process(x) * signL;
        float yR = mR.process(x) * signR;

        // tape-style cross-coupled regen
        fb = softsat_tanh(fb * 0.98f + 0.7f * (yL + yR) * feedback, 0.45f);

        // stereo image
        float L = yL + fb;
        float R = yR - fb;

        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R) * width;

        float t = mix * float(M_PI_2);
        return x * std::cos(t) + (mid + side) * std::sin(t);
    }

private:
    DDXBBDDelay mL, mR;

    float baseDelay = 0.0011f;
    float phase = 0.f;

    float rate = 0.12f;
    float depth = 0.9f;
    float feedback = 0.65f;
    float width = 0.9f;
    float mix = 0.5f;

    float wowAmt = 0.18f;
    float flutAmt = 0.7f;
    float driftAmt = 0.15f;

    float wowPhase = 0.f;
    float flutPhase = 0.f;
    float driftState = 0.f;
    float fb = 0.f;

    static float clamp01(float x){ return std::clamp(x,0.f,1.f); }

    static float mapExp(float n, float a, float b)
    {
        n = clamp01(n);
        float k = (std::exp(2.2f*n)-1.f)/(std::exp(2.2f)-1.f);
        return a + (b-a)*k;
    }

    static inline float rand01()
    {
        static uint32_t s = 22222;
        s = s * 196314165u + 907633515u;
        return float(s & 0x7fffffff) / float(0x7fffffff);
    }
};
