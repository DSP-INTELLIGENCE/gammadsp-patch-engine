class ModRotarySpeaker : public Function
{
public:
    ModRotarySpeaker()
    {
        setMix(0.75f);
        setWidth(1.0f);
        setCrossover(800.f);

        setDrumSpeed(0.8f, 5.0f);
        setHornSpeed(1.2f, 6.2f);

        setDrumDepth(0.0012f);
        setHornDepth(0.0025f);

        setAccel(0.6f);

        setDrumLevel(0.7f);
        setHornLevel(0.9f);
    }

    void setMix(float m)      { mix = std::clamp(m, 0.f, 1.f); }
    void setWidth(float w)    { width = std::clamp(w, 0.f, 1.f); }

    void setCrossover(float f)
    {
        crossover = std::clamp(f, 100.f, 5000.f);
        lowPass.setFreq(crossover);
    }

    void setDrumSpeed(float slow, float fast)
    {
        drumSlow = slow;
        drumFast = fast;
    }

    void setHornSpeed(float slow, float fast)
    {
        hornSlow = slow;
        hornFast = fast;
    }

    void setDrumDepth(float d) { drumDepth = d; }
    void setHornDepth(float d) { hornDepth = d; }

    void setAccel(float a)     { accel = std::clamp(a, 0.05f, 5.f); }

    void setDrumLevel(float l) { drumLevel = l; }
    void setHornLevel(float l) { hornLevel = l; }

    void setFast(bool f)       { fast = f; }

    std::pair<float,float> processStereo(float input)
    {
        // Split bands
        float low  = lowPass.process(input);
        float high = input - low;

        // Speed inertia
        float targetDrum = fast ? drumFast : drumSlow;
        float targetHorn = fast ? hornFast : hornSlow;

        drumSpeed += (targetDrum - drumSpeed) * accel * invSR;
        hornSpeed += (targetHorn - hornSpeed) * accel * invSR;

        // Advance phases
        drumPhase += drumSpeed * invSR;
        hornPhase += hornSpeed * invSR;

        wrap(drumPhase);
        wrap(hornPhase);

        // Doppler
        float drumMod = std::sin(TAU * drumPhase) * drumDepth;
        float hornMod = std::sin(TAU * hornPhase) * hornDepth;

        drumDelay.setDM(drumMod);
        hornDelay.setDM(hornMod);

        float d = drumDelay.update(low)  * drumLevel;
        float h = hornDelay.update(high) * hornLevel;

        // Amplitude beaming
        float drumAmp = 0.5f + 0.5f * std::cos(TAU * drumPhase);
        float hornAmp = 0.5f + 0.5f * std::cos(TAU * hornPhase);

        d *= drumAmp;
        h *= hornAmp;

        // Stereo panning
        auto dlr = pan(d, std::sin(TAU * drumPhase), width);
        auto hlr = pan(h, std::sin(TAU * hornPhase), width);

        float wetL = dlr.first + hlr.first;
        float wetR = dlr.second + hlr.second;

        float outL = input * (1.f - mix) + wetL * mix;
        float outR = input * (1.f - mix) + wetR * mix;

        return { outL, outR };
    }

private:
    static constexpr float TAU = 6.2831853f;
    static constexpr float invSR = 1.f / 48000.f; // replace with gam::sampleRate()

    static void wrap(float& p) { if(p >= 1.f) p -= 1.f; }

    static std::pair<float,float> pan(float s, float pos, float width)
    {
        pos *= width;
        float a = 0.5f * (pos + 1.f);
        float L = s * std::cos(a * 1.5707963f);
        float R = s * std::sin(a * 1.5707963f);
        return { L, R };
    }

    // DSP blocks
    OnePole lowPass;
    ModDelay drumDelay, hornDelay;

    // Parameters
    float mix = 0.75f;
    float width = 1.f;
    float crossover = 800.f;

    float drumSlow = 0.8f, drumFast = 5.0f;
    float hornSlow = 1.2f, hornFast = 6.2f;

    float drumDepth = 0.0012f, hornDepth = 0.0025f;
    float drumLevel = 0.7f, hornLevel = 0.9f;

    float accel = 0.6f;
    bool fast = false;

    // State
    float drumSpeed = 0.f, hornSpeed = 0.f;
    float drumPhase = 0.f, hornPhase = 0.f;
};
