#pragma once
#include "Biquad.hpp"

class BellFilter: public Function {
public:
    BellFilter()     
    {
        lowpass.setType(gam::FilterType::LOW_PASS);
        highpass.setType(gam::FilterType::HIGH_PASS);
        lowpass.setFreq(2000.f);
        highpass.setFreq(500.f);
        setBRM(0.0f);
        setRLM(0.0f);
    }

    void setBrightness(float b)
    {
        // 0 = dark horn, 1 = bright trumpet
        brightness = std::clamp(b, 0.f, 1.f);

        float lp = 800.f + brightness * 6000.f;
        float hp = 50.f  + brightness * 1500.f;

        lowpass.setFreq(lp);
        highpass.setFreq(hp);
    }

    void setBRM(float f) { m_br.set(f); }
    void setRLM(float f) { m_rl.set(f); }

    float process(float x)
    {
        // Separate reflected vs radiated energy
        float reflected = lowpass(x);
        float radiated  = highpass(x);
        float br = m_br.process(brightness);
        if(br != brightness) setBrightness(br);
        return reflected - m_rl.process(radiated * radiationLoss);
    }

    Biquad lowpass;
    Biquad highpass;

    float brightness = 0.5f;
    float radiationLoss = 0.4f;

    Modulator m_br;
    Modulator m_rl;
};
