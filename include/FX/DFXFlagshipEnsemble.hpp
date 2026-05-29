class DFXFlagshipEnsemble {
public:
    DFXFlagshipEnsemble(int voices = 3)
    {
        setVoices(voices);
        setRate(0.30f);
        setDepth(0.006f);
        setBaseDelay(0.018f);
        setMix(0.45f);
        setWidth(0.9f);
    }

    void setVoices(int v)
    {
        v = std::clamp(v, 3, 6);
        voices = v;
        engines.clear();

        for (int i = 0; i < voices; ++i)
        {
            engines.emplace_back(0.05f, 0.02f + i * 0.001f);
            engines.back().setFeedback(0.f);
        }

        // precompute phase offsets
        phaseOffsets.resize(voices);
        for (int i = 0; i < voices; ++i)
            phaseOffsets[i] = 2.f * float(M_PI) * i / float(voices);
    }

    void setRate(float r)      { rate = r; }
    void setDepth(float d)     { depth = d; }
    void setBaseDelay(float d) { baseDelay = d; }
    void setMix(float m)       { mix = std::clamp(m, 0.f, 1.f); }
    void setWidth(float w)     { width = std::clamp(w, 0.f, 1.f); }

    void process(float inL, float inR, float& outL, float& outR)
    {
        float yL = 0.f, yR = 0.f;

        for (int i = 0; i < voices; ++i)
        {
            float lfo = std::sin(phase + phaseOffsets[i]);

            engines[i].lfo.set(lfo);
            engines[i].depth.set(depth);

            // slight per-voice offset
            engines[i].setDelay(baseDelay + i * 0.0007f);
            engines[i].setMix(mix);

            float mono = 0.5f * (inL + inR);
            float v = engines[i].process(mono);

            // stereo spread per voice
            float pan = float(i) / float(voices - 1) * 2.f - 1.f;
            float L = v * (0.5f - 0.5f * pan);
            float R = v * (0.5f + 0.5f * pan);

            yL += L;
            yR += R;
        }

        yL /= voices;
        yR /= voices;

        // width control (mid/side)
        float mid = 0.5f * (yL + yR);
        float side = 0.5f * (yL - yR) * width;

        outL = mid + side;
        outR = mid - side;

        advanceLFO();
    }

private:
    void advanceLFO()
    {
        phase += 2.f * float(M_PI) * rate / gam::sampleRate();
        if (phase > 2.f * float(M_PI)) phase -= 2.f * float(M_PI);
    }

    std::vector<DigitalDelay> engines;
    std::vector<float> phaseOffsets;

    int voices = 3;

    float rate = 0.3f;
    float depth = 0.006f;
    float baseDelay = 0.018f;
    float mix = 0.45f;
    float width = 0.9f;

    float phase = 0.f;
};
