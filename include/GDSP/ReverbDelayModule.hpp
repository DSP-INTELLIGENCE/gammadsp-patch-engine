// ReverbDelayModule.hpp
#pragma once
#include <algorithm>
#include <cstddef>
#include "GammaDSP.hpp"
#include "GDSP_Reverb.hpp"

enum class RDRoute {
    SerialDelayIntoReverb,   // input -> delay(wet) -> reverb(wet)
    Parallel                 // input -> delay(wet) + reverb(wet)
};

class ReverbDelayModule {
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;

        // IMPORTANT: run internal modules WET-ONLY
        delay  = Delay(sr, 2.0f, 0.25f);
        delay.setMix(1.f);

        reverb.prepare(sr);
        reverb.setMix(1.f); // wet-only

        // defaults
        setMix(1.f);
        setDelayMix(1.f);       // stage wet amount (post-delay wet gain)
        setReverbSend(1.f);     // stage wet amount (post-reverb wet gain)
        setRoute(RDRoute::SerialDelayIntoReverb);
        setReverbOnRepeatsOnly(false);
    }

    // routing
    void setRoute(RDRoute r) { route = r; }
    void setReverbOnRepeatsOnly(bool b) { reverbOnRepeatsOnly = b; }

    // delay params
    void setDelayTime(float s)     { delay.setTime(s); }
    void setDelayFeedback(float f) { delay.setFeedback(std::clamp(f, 0.f, 0.97f)); }

    // IMPORTANT: In this module, delay "mix" means wet level of the delay stage (NOT internal dry/wet)
    void setDelayMix(float m)      { delayWetLevel = std::clamp(m, 0.f, 1.f); }

    // reverb params
    // IMPORTANT: reverbMix is wet level of the reverb stage (NOT internal dry/wet)
    void setReverbSend(float m)     { reverbWetLevel = std::clamp(m, 0.f, 1.f); }

    void setReverbSize(float s)     { reverb.setSize(s); }
    void setReverbDecay(float d)    { reverb.setDecay(d); }
    void setReverbDamping(float d)  { reverb.setDamping(d); }
    void setReverbPreDelay(float t) { reverb.setPreDelay(t); }
    void setReverbMode(ReverbMode m){ reverb.setMode(m); }

    // global output mix (whole effect)
    void setMix(float m) { mix = std::clamp(m, 0.f, 1.f); }

    float process(float x)
    {
        const float dry = x;

        // Compute wet components (modules are wet-only internally)
        float wetDelay = 0.f;
        float wetReverb = 0.f;

        if(route == RDRoute::SerialDelayIntoReverb)
        {
            // Delay sees dry input
            wetDelay = delay.process(dry) * delayWetLevel;

            // Reverb input depends on "reverbOnRepeatsOnly"
            float revIn = reverbOnRepeatsOnly ? wetDelay : (dry + wetDelay);

            // Reverb is wet-only
            wetReverb = reverb.process(revIn) * reverbWetLevel;

            // Output for serial:
            // - always include wetDelay (echo)
            // - include wetReverb (tail)
            float y = dry + wetDelay + wetReverb;

            return dry * (1.f - mix) + y * mix;
        }
        else // Parallel
        {
            // Both see the same dry input
            wetDelay  = delay.process(dry) * delayWetLevel;
            wetReverb = reverb.process(dry) * reverbWetLevel;

            float y = dry + wetDelay + wetReverb;
            return dry * (1.f - mix) + y * mix;
        }
    }

    void processBlock(const float* in, float* out, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    float sr = 44100.f;

    // Internal modules are WET-ONLY in this composite
    Delay delay {44100.f, 2.0f, 0.25f};
    ReverbModule reverb;

    RDRoute route = RDRoute::SerialDelayIntoReverb;
    bool reverbOnRepeatsOnly = false;

    // Stage wet levels (owned by this composite)
    float delayWetLevel  = 1.f;
    float reverbWetLevel = 1.f;

    // Overall wet blend
    float mix = 1.0f;
};
