class DFXFlagshipChorus {
public:
    DFXFlagshipChorus()
    : left (0.05f, 0.020f),
      right(0.05f, 0.021f)
    {
        left.setFeedback(0.0f);
        right.setFeedback(0.0f);

        setRate(0.30f);
        setDepth(0.006f);
        setBaseDelay(0.018f);
        setMix(0.45f);
        setWidth(0.85f);
    }

    void setRate(float hz)       { rate = hz; }
    void setDepth(float seconds){ depth = seconds; }
    void setBaseDelay(float s)   { baseDelay = s; }
    void setMix(float m)         { mix = std::clamp(m,0.f,1.f); }
    void setWidth(float w)       { width = std::clamp(w,0.f,1.f); }

    float processL(float xL) { return outL = left.process(xL); }
    float processR(float xR) { return outR = right.process(xR); }

    void process(float inL, float inR, float& oL, float& oR)
    {
        float lfo = std::sin(phase);
        phase += 2.f * float(M_PI) * rate / gam::sampleRate();
        if (phase > 2.f * float(M_PI)) phase -= 2.f * float(M_PI);

        // decorrelated stereo
        float lfoR = std::sin(phase + float(M_PI_2));

        // apply modulation to the universal engine
        left.setDelay(baseDelay);
        right.setDelay(baseDelay);

        left.lfo.set(lfo);
        right.lfo.set(lfoR);

        left.depth.set(depth);
        right.depth.set(depth);

        left.setMix(mix);
        right.setMix(mix);

        float yL = left.process(inL);
        float yR = right.process(inR);

        // width control (mid/side)
        float mid = 0.5f * (yL + yR);
        float side = 0.5f * (yL - yR) * width;

        oL = mid + side;
        oR = mid - side;
    }

private:
    DigitalDelay left, right;

    float rate      = 0.3f;
    float depth     = 0.006f;
    float baseDelay = 0.018f;
    float mix       = 0.45f;
    float width     = 0.85f;

    float phase = 0.f;

    float outL = 0.f, outR = 0.f;
};
