#pragma once

class DattorroVerb {
public:
    DattorroVerb()
    : in1(0.05f), in2(0.05f), in3(0.05f), in4(0.05f),
      apL1(0.05f), apL2(0.05f), apR1(0.05f), apR2(0.05f),
      dL1(0.2f), dL2(0.2f), dR1(0.2f), dR2(0.2f)
    {
        // Input diffusion
        in1.setDelay(0.0048f); in1.setAllPass(0.75f);
        in2.setDelay(0.0035f); in2.setAllPass(0.75f);
        in3.setDelay(0.0027f); in3.setAllPass(0.75f);
        in4.setDelay(0.0018f); in4.setAllPass(0.75f);

        // Tank diffusion
        apL1.setDelay(0.022f); apL1.setAllPass(0.7f);
        apL2.setDelay(0.030f); apL2.setAllPass(0.7f);
        apR1.setDelay(0.033f); apR1.setAllPass(0.7f);
        apR2.setDelay(0.037f); apR2.setAllPass(0.7f);

        // Long delays (tank)
        dL1.setDelay(0.060f);
        dL2.setDelay(0.089f);
        dR1.setDelay(0.073f);
        dR2.setDelay(0.097f);
    }

    void setDecay(float d) { mDecay = std::clamp(d, 0.2f, 0.99f); }

    StereoSample process(float x)
    {
        // Input diffusion
        float y = in4.process(in3.process(in2.process(in1.process(x))));        
        // Tank feedback
        float fbL = m_decayL.process((dR2.read() + dR1.read()) * 0.5f * mDecay);
        float fbR = m_decayR.process((dL2.read() + dL1.read()) * 0.5f * mDecay);

        // Left tank
        float l = apL2.process(dL2.process(apL1.process(dL1.process(m_fbmL.process(y + fbL)))));

        // Right tank
        float r = apR2.process(dR2.process(apR1.process(dR1.process(m_fbmR.process(y + fbR)))));

        return StereoSample(l,r);
    }

    float mDecay = 0.85f;

    Modulator m_decayL, m_decayR;    
    Modulator m_fbmL, m_fbmR;

private:
    Comb in1, in2, in3, in4;
    Comb apL1, apL2, apR1, apR2;
    Comb dL1, dL2, dR1, dR2;
    
};

