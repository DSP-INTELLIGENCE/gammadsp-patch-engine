#pragma once
#include <vector>
#include <algorithm>

class AllPassPhaser : public Function
{
public:
    AllPassPhaser(int stages = 6)
    : baseFreq(400.f),
      baseFB(0.7f),
      baseMix(0.7f)
    {        
        setDepth(0.8f);
        setIM(1.f);
        setAM(1.f);
        setFeedbackMod(0);
        for (int i = 0; i < stages; ++i)
            chain.emplace_back(baseFreq);
    }

    // ---------- Musical Controls ----------    
    
    void setFreq(float v)    { baseFreq = v; }
    void setDepth(float d)   { baseDepth = std::clamp(d, 0.f, 1.f); }
    void setFeedback(float f){ baseFB = std::clamp(f, -0.99f, 0.99f); }
    void setFeedbackMod(float f){ baseFeedbackMod = f; }
    void setMix(float m)     { baseMix = std::clamp(m, 0.f, 1.f); }

    void setLFO(float v) { m_lfo.set(v); }
    void setFM(float v) { m_freq.set(v); }
    void setIM(float v) { m_im.set(v); }
    void setAM(float v) { m_am.set(v); }

    float process(float x) override
    {
        // LFO        
        float lfo = m_lfo.process();

        // Sweep
        float sweep = m_freq.process(baseFreq) * std::pow(2.f, lfo * m_depth.process(baseDepth) * 3.f);
        sweep = std::clamp(sweep, 40.f, 0.45f * (float)gam::sampleRate());

        m_am.set(last * baseFeedbackMod);        
        float in = x + last * m_fb.process(baseFB);
        lastIn = x;
        for (auto& s : chain) {
            s.setCutoff(sweep);
            in = s.process(in);
        }
        
        float mix = m_mix.process(baseMix);
        float out = x * (1.f - mix) + in * mix;
        last = out;
        //m_am.set(last * baseFeedbackMod);
        return m_am.process(out);
    }

    Modulator m_freq, m_fb, m_mix, m_depth, m_lfo, m_im, m_am;
    float baseFreq;
    float baseFB;
    float baseMix;
    float baseDepth;
    float baseFeedbackMod;
    
private:
    std::vector<ModAllPass1> chain;

    float last  = 0.f;
    float lastIn =0.f;
};
