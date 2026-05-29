#pragma once
#include <algorithm>
#include <cmath>
#include "DattorroVerb.hpp"

// ------------------------------------------------------------
// LexiconHall
// Dattorro-based hall with Lexicon-style upgrades
// ------------------------------------------------------------
class LexiconHall
{
public:
    LexiconHall()
    {
        // ---- Default tuning (Lexicon Hall style) ----
        setDecay(0.88f);

        setHF(8000.f);
        setLF(350.f);

        setHFDecay(0.72f);
        setLFDecay(0.93f);

        setModRate(0.25f);
        setModDepth(0.0007f); // ~0.7 ms

        width = 1.2f;
        mix   = 1.0f;
    }

    // ---------------- Controls ----------------

    void setDecay(float d)      { verb.mDecay = std::clamp(d, 0.2f, 0.99f); }
    void setHF(float hz)        { hfFreq = hz; updateFilters(); }
    void setLF(float hz)        { lfFreq = hz; updateFilters(); }
    void setHFDecay(float v)    { hfDecay = std::clamp(v, 0.f, 1.f); }
    void setLFDecay(float v)    { lfDecay = std::clamp(v, 0.f, 1.f); }

    void setModRate(float hz)
    {
        lfoL.setFreq(std::max(0.f, hz));
        lfoR.setFreq(std::max(0.f, hz * 1.17f)); // intentional asymmetry
    }

    void setModDepth(float sec)
    {
        modDepth = std::max(0.f, sec);
    }

    void setWidth(float w) { width = std::clamp(w, 0.f, 2.f); }
    void setMix(float m)   { mix   = std::clamp(m, 0.f, 1.f); }

    // ---------------- Processing ----------------

    StereoSample process(float input)
    {
        // ---------- LFO modulation ----------
        float mL = lfoL.process() * modDepth;
        float mR = lfoR.process() * modDepth;

        // Inject modulation directly into tank delays
        verb.dL1.setDelay(0.060f + mL);
        verb.dL2.setDelay(0.089f - mL);

        verb.dR1.setDelay(0.073f + mR);
        verb.dR2.setDelay(0.097f - mR);

        // ---------- Input diffusion + tank ----------
        float y = verb.in4.process(
                    verb.in3.process(
                      verb.in2.process(
                        verb.in1.process(input))));

        // ---------- Tank feedback (raw) ----------
        float fbInL = (verb.dR1.read() + verb.dR2.read()) * 0.5f;
        float fbInR = (verb.dL1.read() + verb.dL2.read()) * 0.5f;

        // ---------- HF decay ----------
        float hfLsig = hfL.process(fbInL);
        float hfRsig = hfR.process(fbInR);

        fbInL = hfDecay * hfLsig + (1.f - hfDecay) * fbInL;
        fbInR = hfDecay * hfRsig + (1.f - hfDecay) * fbInR;

        // ---------- LF control ----------
        float lfLsig = lfL.process(fbInL);
        float lfRsig = lfR.process(fbInR);

        fbInL = lfDecay * lfLsig + (1.f - lfDecay) * fbInL;
        fbInR = lfDecay * lfRsig + (1.f - lfDecay) * fbInR;

        // ---------- Apply decay + cross-coupling ----------
        float fbL = (fbInL + 0.15f * fbInR) * verb.mDecay;
        float fbR = (fbInR + 0.15f * fbInL) * verb.mDecay;

        // ---------- Left / Right tanks ----------
        float outL =
            verb.apL2.process(
                verb.dL2.process(
                    verb.apL1.process(
                        verb.dL1.process(y + fbL))));

        float outR =
            verb.apR2.process(
                verb.dR2.process(
                    verb.apR1.process(
                        verb.dR1.process(y + fbR))));

        // ---------- Stereo width (mid/side) ----------
        float mid  = 0.5f * (outL + outR);
        float side = 0.5f * (outL - outR) * width;

        float wetL = mid + side;
        float wetR = mid - side;

        // ---------- Wet / Dry ----------
        StereoSample s;
        s.outL = input * (1.f - mix) + wetL * mix;
        s.outR = input * (1.f - mix) + wetR * mix;
        return s;
    }

private:
    void updateFilters()
    {
        hfL.setType(gam::LOW_PASS);
        hfR.setType(gam::LOW_PASS);
        hfL.setFreq(hfFreq);
        hfR.setFreq(hfFreq);

        lfL.setType(gam::HIGH_PASS);
        lfR.setType(gam::HIGH_PASS);
        lfL.setFreq(lfFreq);
        lfR.setFreq(lfFreq);
    }


private:
    // Exposed for advanced tweaking if desired
    DattorroVerb verb;

    // Frequency-dependent decay
    OnePole hfL, hfR;
    OnePole lfL, lfR;

    float hfFreq  = 8000.f;
    float lfFreq  = 350.f;
    float hfDecay = 0.72f;
    float lfDecay = 0.93f;

    // Modulation
    LFO   lfoL {0.25f};
    LFO   lfoR {0.29f};
    float modDepth = 0.0007f;

    // Global
    float width = 1.2f;
    float mix   = 1.0f;
};
