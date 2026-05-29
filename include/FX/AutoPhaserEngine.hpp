#pragma once
#include "Phaser.hpp"
#include <cmath>

class AutoPhaserEngine : public Function
{
public:
    AutoPhaserEngine()
    {
        setRate(0.25f);
        setDepth(0.9f);
        setEnvAmount(0.7f);
        setFeedback(0.45f);
        setMix(0.7f);
        setEnvTime(0.05f);
    }

    void setRate(float r)      { m_rate.set(r); }
    void setDepth(float d)     { m_depth = std::clamp(d, 0.f, 1.f); }
    void setEnvAmount(float e) { m_envAmt = std::clamp(e, 0.f, 1.f); }
    void setFeedback(float f)  { m_phaser.setFeedback(f); }
    void setMix(float m)       { m_phaser.setMix(m); }

    void setEnvTime(float t)
    {
        m_envCoef = std::exp(-1.0f / (t * gam::sampleRate()));
    }

    float process(float x) override
    {
        // ---------- Envelope follower ----------
        float level = std::fabs(x);
        m_env += (level - m_env) * (1.f - m_envCoef);

        // ---------- Motion LFO ----------
        float lfoPhase = m_rate.process();
        float lfo = std::sin(lfoPhase * 6.2831853f);

        // ---------- Motion blend ----------
        float motion = lfo * m_depth + m_env * m_envAmt;
        motion = std::clamp(motion, -1.f, 1.f);

        // ---------- Drive phaser ----------
        m_phaser.setDepth(std::fabs(motion));
        return m_phaser.process(x);
    }

private:
    Phaser m_phaser;

    Modulator m_rate;

    float m_depth  = 0.9f;
    float m_envAmt = 0.7f;

    float m_env    = 0.0f;
    float m_envCoef = 0.001f;
};
