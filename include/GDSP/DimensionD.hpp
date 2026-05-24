#pragma once

class DimensionD : public TModule {
public:
    enum Mode { OFF = 0, I, II, III, IV };

    DimensionD(float maxDelay = 0.05f)
    : delayA(maxDelay, 0.020f),
      delayB(maxDelay, 0.020f),
      lfoA(0.25f),
      lfoB(0.50f)
    {
        delayA.setIpolType(gam::ipl::Type::CUBIC);
        delayB.setIpolType(gam::ipl::Type::CUBIC);

        lfoA.setWave(LFO::SINE);
        lfoB.setWave(LFO::SINE);

        lfoA.setUnipolar(false);
        lfoB.setUnipolar(false);

        setMode(I);
    }

    void setMode(Mode m)
    {
        mode = m;
        switch (mode)
        {
            case I:   rateA=0.25f; rateB=0.50f; depth=0.00022f; break;
            case II:  rateA=0.30f; rateB=0.60f; depth=0.00025f; break;
            case III: rateA=0.35f; rateB=0.70f; depth=0.00028f; break;
            case IV:  rateA=0.40f; rateB=0.80f; depth=0.00032f; break;
            default: break;
        }

        lfoA.setFreq(rateA);
        lfoB.setFreq(rateB);
    }

    StereoSample process(float inL, float inR)
    {
        if (mode == OFF)
            return { inL, inR };

        // ---- LFOs ----
        float lA_L = lfoA.process();
        float lB_L = lfoB.process();

        lfoA.phaseAdd(halfPi);
        lfoB.phaseAdd(halfPi);

        float lA_R = lfoA.process();
        float lB_R = lfoB.process();

        lfoA.phaseAdd(-halfPi);
        lfoB.phaseAdd(-halfPi);

        // ---- Delay times ----
        float dA_L = baseDelay + depth * lA_L;
        float dB_L = baseDelay + depth * lB_L;
        float dA_R = baseDelay + depth * lA_R;
        float dB_R = baseDelay + depth * lB_R;

        // ---- Left channel ----
        delayA.setDelay(dA_L);
        delayB.setDelay(dB_L);
        float aL = delayA.process(inL);
        float bL = delayB.process(inL);

        // ---- Right channel ----
        delayA.setDelay(dA_R);
        delayB.setDelay(dB_R);
        float aR = delayA.process(inR);
        float bR = delayB.process(inR);

        // ---- Stereo cross-mix (Dimension matrix) ----
        float outL = inL + aL + 0.5f * bR;
        float outR = inR + aR + 0.5f * bL;

        return { outL, outR };
    }

private:
    Mode mode = I;

    Delay delayA;
    Delay delayB;

    LFO lfoA;
    LFO lfoB;

    float baseDelay = 0.020f;
    float depth     = 0.00025f;
    float rateA     = 0.25f;
    float rateB     = 0.50f;

    static constexpr float halfPi = 1.57079632679f;
};
