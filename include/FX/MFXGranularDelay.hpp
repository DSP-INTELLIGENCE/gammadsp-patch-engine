#pragma once
#include "GDSP_ModDelay.hpp"

class ModGranularDelay : public Function
{
public:
    ModGranularDelay()
    {
        setTime(0.5f);
        setGrainSize(0.08f);
        setDensity(0.6f);
        setPitch(0.0f);
        setMix(0.7f);
        setFeedback(0.4f);
    }

    void setTime(float t)       { delayTime = std::clamp(t, 0.05f, 2.0f); }
    void setGrainSize(float g)  { grainSize = std::clamp(g, 0.01f, 0.2f); }
    void setDensity(float d)    { density = std::clamp(d, 0.0f, 1.0f); }
    void setPitch(float p)      { pitch = p; }
    void setMix(float m)        { mix = std::clamp(m, 0.f, 1.f); }
    void setFeedback(float f)   { feedback = std::clamp(f, 0.f, 0.98f); }

    float process(float input) override
    {
        size_t bufLen = (size_t)(maxDelay * gam::sampleRate());
        if (buffer.size() != bufLen)
            buffer.assign(bufLen, 0.f);

        buffer[writePos] = input + fbSig * feedback;

        float out = 0.f;

        spawnCounter += density;
        if (spawnCounter >= 1.f)
        {
            spawnCounter -= 1.f;
            spawnGrain();
        }

        for (auto& g : grains)
            out += g.process(buffer, bufLen);

        writePos = (writePos + 1) % bufLen;

        fbSig = out;

        return input * (1.f - mix) + out * mix;
    }

private:
    struct Grain
    {
        size_t pos = 0;
        float phase = 0.f;
        float size = 0.f;
        float pitch = 1.f;
        bool active = false;

        float process(const std::vector<float>& buf, size_t len)
        {
            if (!active) return 0.f;

            float env = 0.5f - 0.5f * std::cos(phase * 6.2831853f);
            float sample = buf[pos];

            pos = (pos + (size_t)pitch) % len;
            phase += 1.f / size;

            if (phase >= 1.f)
                active = false;

            return sample * env;
        }
    };

    void spawnGrain()
    {
        for (auto& g : grains)
        {
            if (!g.active)
            {
                g.active = true;
                g.size = grainSize * gam::sampleRate();
                g.phase = 0.f;
                g.pos = (writePos + buffer.size()
                        - (size_t)(delayTime * gam::sampleRate())) % buffer.size();
                g.pitch = std::pow(2.f, pitch / 12.f);
                return;
            }
        }
    }

    static constexpr int MAX_GRAINS = 24;

    std::array<Grain, MAX_GRAINS> grains;

    std::vector<float> buffer;

    size_t writePos = 0;
    float spawnCounter = 0.f;

    float delayTime = 0.5f;
    float grainSize = 0.08f;
    float density = 0.6f;
    float pitch = 0.f;
    float mix = 0.7f;
    float feedback = 0.4f;
    float fbSig = 0.f;

    float maxDelay = 3.0f;
};
