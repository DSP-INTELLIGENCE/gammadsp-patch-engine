class ModStereoWidener
{
public:
    ModStereoWidener()
    {
        setWidth(1.2f);
        setDecorrelate(0.002f);
        setMix(1.0f);

        decorDelay.setMM(1.0f);
        decorDelay.setFM(0.0f);
    }

    void setWidth(float w)      { width = std::clamp(w, 0.f, 2.f); }
    void setDecorrelate(float d){ decorDepth = std::clamp(d, 0.f, 0.01f); }
    void setMix(float m)        { mix = std::clamp(m, 0.f, 1.f); }

    std::pair<float,float> process(float L, float R)
    {
        // Mid / Side
        float mid  = 0.5f * (L + R);
        float side = 0.5f * (L - R);

        // Micro decorrelation on side
        decorDelay.setDelay(decorDepth);
        float decor = decorDelay.update(side);

        side = decor * width;

        // Back to L/R
        float outL = mid + side;
        float outR = mid - side;

        outL = L * (1.f - mix) + outL * mix;
        outR = R * (1.f - mix) + outR * mix;

        return { outL, outR };
    }

private:
    float width = 1.2f;
    float decorDepth = 0.002f;
    float mix = 1.0f;

    ModDelay decorDelay;
};
