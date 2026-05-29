// DispersionNetwork.hpp
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

#include "Engine.hpp"
#include "Parameters.hpp"

#include "ModAllPass2.hpp"
#include "AllPass1.hpp"
#include "OnePole.hpp"

class DispersionNetwork : public Function
{
public:
    DispersionNetwork(int layers = 3, int stagesPerLayer = 8)
    : baseFreq(600.f),
      baseDispersion(0.7f),
      baseFB(0.6f),
      baseMix(0.8f),
      drive(1.2f),
      // loop loss (physical)
      loopLP(8000.f, gam::LOW_PASS),
      loopHP(80.f,   gam::HIGH_PASS)
    {
        setRate(0.12f);
        setDepth(0.9f);
        setDispersion(baseDispersion);
        setFeedback(baseFB);
        setMix(baseMix);
        setDrive(drive);

        setDamping(0.6f);   // 0..1 -> LP cutoff
        setHighpass(80.f);  // rumble control
        setIM(1.f);
        setAM(1.f);

        // early diffusion (optional but helps “space”)
        for (auto& ap : early)
            ap.setFreq(1400.f);

        // allocate fabric: layers × stages
        fabric.resize(std::max(1, layers));
        for (auto& layer : fabric)
        {
            layer.reserve(std::max(1, stagesPerLayer));
            for (int i = 0; i < std::max(1, stagesPerLayer); ++i)
                layer.emplace_back(baseFreq, /*width*/ 120.f);
        }
    }

    // -------- Controls --------
    void setRate(float r)       { rate.set(std::max(0.01f, r)); }
    void setDepth(float d)      { depth = std::clamp(d, 0.f, 1.f); }

    void setDispersion(float d) { baseDispersion = std::clamp(d, 0.f, 1.f); }

    // Outer energy feedback (material sustain)
    void setFeedback(float f)   { baseFB = std::clamp(f, -0.99f, 0.99f); }

    void setMix(float m)        { baseMix = std::clamp(m, 0.f, 1.f); }
    void setDrive(float d)      { drive = std::max(0.f, d); }

    // Loop loss shaping
    void setDamping(float d)    // 0..1 => LP 2k..12k
    {
        damping = std::clamp(d, 0.f, 1.f);
        float hz = 2000.f + damping * 10000.f;
        loopLP.setFreq(hz);
    }

    void setHighpass(float hz)
    {
        hpHz = hz;
        loopHP.setFreq(hz);
    }

    // Optional “space diffusion” (0..1 maps to how hard the early APs work)
    void setDiffusion(float d)
    {
        diffusion = std::clamp(d, 0.f, 1.f);
        // We only have AllPass1 freq here; raise freq for less audible phasing,
        // lower freq for thicker smear.
        float f = 900.f + (1.f - diffusion) * 2200.f; // diffusion↑ => lower freq => more smear
        for (auto& ap : early) ap.setFreq(f);
    }

    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    void reset()
    {
        last = 0.f;
        phase = 0.f;
        loopLP.reset(0.f);
        loopHP.reset(0.f);
        // ModAllPass2 has reset()
        for (auto& layer : fabric)
            for (auto& st : layer)
                st.reset();
        for (auto& ap : early)
            ap.reset();
    }

    float process(float x) override
    {
        const float sr = (float)gam::sampleRate();

        // LFO (engine-style)
        float r = rate.process();
        phase += r / sr;
        if (phase >= 1.f) phase -= 1.f;

        float lfo = std::sin(phase * 6.2831853f);
        float motion = lfo * depth;

        // center frequency follows motion
        float center = baseFreq * std::pow(2.f, motion * 3.f);
        center = std::clamp(center, 40.f, 0.45f * sr);

        // --- feedback injection (nonlinear, then physical loss) ---
        // Start from last output (state), apply feedback gain and drive
        float fb = last * baseFB;

        // remove rumble / DC buildup first (more stable in long feedback)
        loopHP.setFreq(hpHz);
        fb = loopHP.process(fb);

        // air/material loss (LP) then saturate
        fb = loopLP.process(fb);
        fb = std::tanh(fb * drive);

        // input injection
        float in = x * im.process() + fb;

        // early diffusion (adds spatial spread before the “material”)
        // can be made optional by setting diffusion=0
        for (auto& ap : early)
            in = ap.process(in);

        // --- dispersive fabric ---
        for (auto& layer : fabric)
        {
            const int N = (int)layer.size();
            const float invNm1 = (N > 1) ? (1.f / (float)(N - 1)) : 0.f;

            for (int i = 0; i < N; ++i)
            {
                float norm = (N > 1) ? (float)i * invNm1 : 0.5f;
                float offset = (norm - 0.5f) * baseDispersion * 4.f;

                // stage frequency (octave spread around center)
                float f = center * std::pow(2.f, offset);
                f = std::clamp(f, 40.f, 0.45f * sr);

                layer[i].setCutoff(f);
                // width can optionally track dispersion for stronger formant-like delay peaks
                // layer[i].setWidth(60.f + baseDispersion * 240.f);

                in = layer[i].process(in);
            }
        }

        // wet/dry + output gain mod
        float out = x * (1.f - baseMix) + in * baseMix;
        out *= am.process();

        last = in;
        return out;
    }

private:
    // fabric: layers × stages
    std::vector<std::vector<ModAllPass2>> fabric;

    // optional early diffusion
    AllPass1 early[4];

    // loop loss
    OnePole loopLP;
    OnePole loopHP;

    // parameters
    float baseFreq;
    float baseDispersion;
    float baseFB;
    float baseMix;
    float drive;

    float depth = 0.9f;
    float damping = 0.6f;
    float diffusion = 0.5f;
    float hpHz = 80.f;

    Modulator rate;
    Modulator im, am;

    float phase = 0.f;
    float last  = 0.f;
};
