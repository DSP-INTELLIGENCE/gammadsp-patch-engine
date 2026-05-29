#pragma once
#include <cmath>
#include <vector>
#include "FrequencyShifter.hpp"

class SpectralPhaser : public Function {
public:
    SpectralPhaser(int stages = 6)
    : shifter(0.f)
    {
        for(int i=0;i<stages;i++)
            ap.emplace_back(600.f + i * 250.f);
    }

    void setRate(float r)  { baseRate = r; }
    void setDepth(float d) { baseDepth = d; }
    void setFeedback(float f) { baseFeedback = std::clamp(f, 0.f, 0.98f); }

    float process(float x) override
    {
        float depth = m_depth.process(baseDepth);
        float rate  = m_rate.process(baseRate);
        float feedback = m_feedback.process(baseFeedback);

        float m = 0.5f + 0.5f * std::sin(phase);        
        phase += 2.f * M_PI * rate / gam::sampleRate();
        m = m_lfo.process(m);
        

        float y = x;

        for(auto& s : ap)
        {
            float f = 300.f + m * 2500.f * depth;
            s.freq(f);
            y = s.process(y);
        }

        shifter.setShift((m - 0.5f) * 30.f * depth);
        float z = shifter.process(y);

        fb = fb * feedback + z;
        return m_am.process(x + fb);
    }

    Modulator m_lfo, m_depth, m_rate, m_feedback, m_am;
    FrequencyShifter shifter;

    float baseRate = 0.2f;
    float baseDepth = 1.0f;
    float baseFeedback = 0.6f;

private:
    std::vector<AllPass1> ap;
    float phase = 0.f;
    float fb = 0.f;    
};
