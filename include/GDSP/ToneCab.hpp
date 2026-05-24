// GDSP_ToneCab.hpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

// ------------------------------------------------------------
// ToneCab: Amp-style tone stack + simple cab/speaker coloration
// ------------------------------------------------------------
// Placement: AFTER your oversampled AnalogStage / Waveshaper.
// Runs at base sample-rate (do NOT oversample this).
//
// Knobs are normalized 0..1 unless noted.
//
// Notes on Gamma filters:
// - gam::Biquad uses .level() for gain-ish parameter; exact semantics depend on type.
// - We keep this class robust even if "level" is not dB; you can retune later.
// - We update coefficients at CONTROL RATE (once per block) via ParamLinear ramping.
// ------------------------------------------------------------

class ToneCab : public Function
{
public:
    enum class ToneType { FENDER, MARSHALL, VOX };
    enum class CabType  { OFF, OPEN, CLOSED, BRIGHT };

    ToneCab()
    : toneStack(3),   // default 3 sections; VOX uses 2 but we can just reuse 3 with a neutral 3rd
      cabLP(), cabHP(), postHP(), postLP(),
      cabRes(110.0f, 140.0f),
      smear(900.0f, 450.0f, 3) // subtle phase smear (3 stages)
    {
        // Default smoothing times
        bassS.setTime(20.0f);
        midS.setTime(20.0f);
        trebleS.setTime(20.0f);
        presenceS.setTime(20.0f);
        cabAmtS.setTime(25.0f);
        mixS.setTime(10.0f);
        lowCutS.setTime(30.0f);
        highCutS.setTime(30.0f);

        // Attach smoothers to params
        bassP.attachSmoother(&bassS);
        midP.attachSmoother(&midS);
        trebleP.attachSmoother(&trebleS);
        presenceP.attachSmoother(&presenceS);
        cabAmtP.attachSmoother(&cabAmtS);
        mixP.attachSmoother(&mixS);
        lowCutP.attachSmoother(&lowCutS);
        highCutP.attachSmoother(&highCutS);

        // Defaults
        setToneType(ToneType::FENDER);
        setCabType(CabType::OPEN);

        setBass(0.5f);
        setMid(0.5f);
        setTreble(0.5f);
        setPresence(0.35f);

        setCabAmount(0.75f);
        setLowCutHz(80.0f);
        setHighCutHz(6500.0f);

        setMix(1.0f);
    }

    // ---------- lifecycle ----------
    void init(float sampleRate)
    {
        sr = std::max(8000.0f, sampleRate);
        // Gamma uses global sample rate; ensure your engine sets it elsewhere.
        // If not, uncomment:
        // gam::sampleRate(sr);

        nyq = 0.5f * sr;

        reset();
        forceUpdateAll();
    }

    void reset()
    {
        toneStack.reset();
        cabLP.reset();
        cabHP.reset();
        postHP.reset();
        postLP.reset();
        cabRes.reset();
        smear.reset();

        // reset smoothing to current values (no jump)
        bassS  = ParamLinear(bassP._value,  bassSTimeMs());
        midS   = ParamLinear(midP._value,   midSTimeMs());
        trebleS= ParamLinear(trebleP._value,trebleSTimeMs());
        presenceS=ParamLinear(presenceP._value,presenceSTimeMs());
        cabAmtS= ParamLinear(cabAmtP._value,cabAmtSTimeMs());
        mixS   = ParamLinear(mixP._value,   mixSTimeMs());
        lowCutS= ParamLinear(lowCutP._value,lowCutSTimeMs());
        highCutS=ParamLinear(highCutP._value,highCutSTimeMs());
    }

    // ---------- tone controls ----------
    void setToneType(ToneType t) { toneType = t; dirtyTone = true; }
    void setCabType(CabType t)   { cabType  = t; dirtyCab  = true; }

    void setBass(float v)     { bassP.set(clamp01(v));     dirtyTone = true; }
    void setMid(float v)      { midP.set(clamp01(v));      dirtyTone = true; }
    void setTreble(float v)   { trebleP.set(clamp01(v));   dirtyTone = true; }
    void setPresence(float v) { presenceP.set(clamp01(v)); dirtyTone = true; } // presence = extra HF tilt

    void setCabAmount(float v){ cabAmtP.set(clamp01(v));   dirtyCab  = true; }

    void setLowCutHz(float hz)
    {
        lowCutP.set(std::clamp(hz, 0.0f, 0.45f * sr));
        dirtyPost = true;
    }

    void setHighCutHz(float hz)
    {
        highCutP.set(std::clamp(hz, 50.0f, 0.45f * sr));
        dirtyPost = true;
    }

    void setMix(float v) { mixP.set(clamp01(v)); }

    // Optional: set smoothing times (ms)
    void setSmoothingMs(float toneMs, float cabMs, float postMs)
    {
        bassS.setTime(toneMs);
        midS.setTime(toneMs);
        trebleS.setTime(toneMs);
        presenceS.setTime(toneMs);
        cabAmtS.setTime(cabMs);
        lowCutS.setTime(postMs);
        highCutS.setTime(postMs);
        mixS.setTime(std::min(10.0f, postMs));
    }

    // ---------- processing ----------
    float process(float x) override
    {
        // 1) run smoothers (control-rate updates happen via flags below)
        const float bass     = bassP.process();
        const float mid      = midP.process();
        const float treble   = trebleP.process();
        const float presence = presenceP.process();
        const float cabAmt   = cabAmtP.process();
        const float mix      = mixP.process();
        const float lowCutHz = lowCutP.process();
        const float highCutHz= highCutP.process();

        // 2) Update coefficients when needed (cheap, but keep off per-sample if possible)
        //    If you're processing block-wise, call updateIfNeeded() once per block instead.
        //    Here we keep it safe for sample-by-sample usage:
        if (dirtyTone || bassS.isRamping() || midS.isRamping() || trebleS.isRamping() || presenceS.isRamping())
            updateToneStack(bass, mid, treble, presence);

        if (dirtyCab || cabAmtS.isRamping())
            updateCab(cabAmt);

        if (dirtyPost || lowCutS.isRamping() || highCutS.isRamping())
            updatePost(lowCutHz, highCutHz);

        // 3) DSP chain
        const float dry = x;

        // Tone stack (SOS)
        float y = toneStack.process(x);

        // Cabinet block (can be bypassed)
        y = processCab(y, cabAmt);

        // Post EQ cleanup
        y = processPost(y);

        // Wet/dry mix
        return dry * (1.0f - mix) + y * mix;
    }

    void run(const float* in, float* out, size_t n)
    {
        // Block-wise coefficient updates: better than per-sample.
        // Advance smoothers across the block, updating coefficients only when needed.
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    // ---------- Helpers ----------
    static float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

    static float knobCurve(float v, float p = 1.6f)
    {
        // Pot-like curve; adjust exponent to taste
        v = clamp01(v);
        return std::pow(v, p);
    }

    // Map 0..1 to a "gain factor" around 1.0.
    // This is deliberately conservative because Gamma's .level() behavior varies by filter type.
    static float gainFromKnob(float v, float range = 1.0f)
    {
        // v=0.5 -> 1.0; v=0 -> 1-range; v=1 -> 1+range
        v = clamp01(v);
        return 1.0f + (2.0f * v - 1.0f) * range;
    }

    // Map 0..1 to a mid-scoop depth (0..1)
    static float scoopFromMid(float mid)
    {
        // Fender: mid knob "fills" scoop; low mid => deep scoop
        mid = clamp01(mid);
        return 1.0f - knobCurve(mid, 1.2f);
    }

    // Map 0..1 to peak gain strength (0..1.5)
    static float peakFromMid(float mid)
    {
        return 0.3f + 1.2f * knobCurve(mid, 1.1f);
    }

    // ---------- Coefficient Updates ----------
    void forceUpdateAll()
    {
        dirtyTone = dirtyCab = dirtyPost = true;

        // Prime once
        updateToneStack(bassP._value, midP._value, trebleP._value, presenceP._value);
        updateCab(cabAmtP._value);
        updatePost(lowCutP._value, highCutP._value);
    }

    void updateToneStack(float bass, float mid, float treble, float presence)
    {
        bass     = knobCurve(bass);
        mid      = knobCurve(mid);
        treble   = knobCurve(treble);
        presence = knobCurve(presence, 1.3f);

        // NOTE: These are "macro" approximations using existing biquads.
        // They are intentionally stable and musical; later you can replace with true WDF stacks.

        // We'll use the SOS sections by configuring each internal Biquad:
        // section[0] = low shelf, section[1] = mid shaping, section[2] = high shelf / presence tilt

        // Ensure we have at least 3
        if (toneStack.mSections.size() < 3)
            toneStack.mSections.resize(3);

        // --- Common base frequencies (tunable) ---
        const float fBassF  = 120.0f;
        const float fMidF   = 420.0f;
        const float fTrebleF= 3500.0f;

        const float fBassM  = 150.0f;
        const float fMidM   = 700.0f;
        const float fTrebleM= 3200.0f;

        const float fBassV  = 180.0f;
        const float fMidV   = 650.0f;
        const float fTrebleV= 2800.0f;

        // --- Gain ranges (conservative) ---
        const float bassRange     = 0.9f;  // +/- 0.9 around 1
        const float trebleRange   = 1.1f;
        const float presenceRange = 0.5f;

        auto& s0 = toneStack.mSections[0];
        auto& s1 = toneStack.mSections[1];
        auto& s2 = toneStack.mSections[2];

        // Default "neutralize" section 2 for VOX if desired (we still use it as presence)
        // Filter types available in Gamma: LOW_SHELF, HIGH_SHELF, PEAKING, BAND_REJECT, BAND_PASS etc.

        switch (toneType)
        {
            case ToneType::FENDER:
            {
                // Low shelf
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassF);
                s0.setRes(0.707f);
                s0.setLevel(gainFromKnob(bass, bassRange));

                // Mid scoop: band reject amount increases as mid decreases
                s1.setType(gam::BAND_REJECT);
                s1.setFreq(fMidF);
                s1.setRes(0.9f);
                // "level" here used as strength; tuned by ear
                s1.setLevel(1.0f + scoopFromMid(mid) * 1.2f);

                // High shelf (treble) + presence tilt
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleF);
                s2.setRes(0.707f);
                s2.setLevel(gainFromKnob(treble, trebleRange) * gainFromKnob(presence, presenceRange));
            } break;

            case ToneType::MARSHALL:
            {
                // Tight bass shelf
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassM);
                s0.setRes(0.707f);
                s0.setLevel(gainFromKnob(bass, 0.8f));

                // Mid peak (Marshall growl)
                s1.setType(gam::PEAKING);
                s1.setFreq(fMidM);
                s1.setRes(1.1f);
                s1.setLevel(peakFromMid(mid));

                // Treble shelf + presence
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleM);
                s2.setRes(0.707f);
                s2.setLevel(gainFromKnob(treble, 1.2f) * gainFromKnob(presence, 0.65f));
            } break;

            case ToneType::VOX:
            {
                // Vox is chimey and less mid-scooped; use gentle bass shelf and high shelf.
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassV);
                s0.setRes(0.707f);
                s0.setLevel(gainFromKnob(bass, 0.7f));

                // Gentle mid shaping (small peak)
                s1.setType(gam::PEAKING);
                s1.setFreq(fMidV);
                s1.setRes(0.8f);
                s1.setLevel(0.9f + 0.7f * mid);

                // Treble shelf + presence
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleV);
                s2.setRes(0.707f);
                s2.setLevel(gainFromKnob(treble, 1.35f) * gainFromKnob(presence, 0.8f));
            } break;
        }

        dirtyTone = false;
    }

    void updateCab(float cabAmt)
    {
        cabAmt = clamp01(cabAmt);

        // If cab is OFF, we still keep filters configured but we will bypass in processCab().
        // Cab voice tuning:
        // - OPEN: looser bass, slightly higher HF cut
        // - CLOSED: tighter bass bump, lower HF cut
        // - BRIGHT: higher HF cut, less rolloff, small presence

        float resF = 110.0f;
        float resW = 160.0f;
        float lpF  = 5600.0f;
        float hpF  = 60.0f;

        float smearFreq  = 900.0f;
        float smearWidth = 500.0f;
        float smearDetune= 0.15f;

        switch (cabType)
        {
            case CabType::OFF:
                // values irrelevant, will bypass
                break;

            case CabType::OPEN:
                resF = 105.0f; resW = 210.0f;
                lpF  = 6000.0f;
                hpF  = 55.0f;
                smearFreq = 900.0f; smearWidth = 650.0f;
                smearDetune = 0.18f;
                break;

            case CabType::CLOSED:
                resF = 130.0f; resW = 140.0f;
                lpF  = 4800.0f;
                hpF  = 70.0f;
                smearFreq = 850.0f; smearWidth = 450.0f;
                smearDetune = 0.12f;
                break;

            case CabType::BRIGHT:
                resF = 115.0f; resW = 170.0f;
                lpF  = 7000.0f;
                hpF  = 65.0f;
                smearFreq = 1100.0f; smearWidth = 520.0f;
                smearDetune = 0.10f;
                break;
        }

        // Clamp to SR
        resF = std::clamp(resF,  5.0f, 0.45f * sr);
        resW = std::clamp(resW,  0.0f, 0.45f * sr);
        lpF  = std::clamp(lpF,  50.0f, 0.45f * sr);
        hpF  = std::clamp(hpF,   0.0f, 0.45f * sr);

        // Apply
        cabRes.setFreq(resF);
        cabRes.setWidth(resW);

        cabLP.freq(lpF);
        cabLP.res(0.707f);

        cabHP.freq(hpF);
        cabHP.res(0.707f);

        smear.freq(smearFreq);
        smear.width(smearWidth);
        smear.setDetune(smearDetune);

        dirtyCab = false;
    }

    void updatePost(float lowCutHz, float highCutHz)
    {
        lowCutHz  = std::clamp(lowCutHz,  0.0f, 0.45f * sr);
        highCutHz = std::clamp(highCutHz, 50.0f, 0.45f * sr);

        if (highCutHz < lowCutHz + 20.0f)
            highCutHz = lowCutHz + 20.0f;

        // HP
        if (lowCutHz <= 1.0f)
        {
            postHpEnabled = false;
        }
        else
        {
            postHpEnabled = true;
            postHP.freq(lowCutHz);
            postHP.res(0.707f);
        }

        // LP
        if (highCutHz >= 0.45f * sr)
        {
            postLpEnabled = false;
        }
        else
        {
            postLpEnabled = true;
            postLP.freq(highCutHz);
            postLP.res(0.707f);
        }

        dirtyPost = false;
    }

    // ---------- DSP blocks ----------
    float processCab(float x, float cabAmt)
    {
        if (cabType == CabType::OFF || cabAmt <= 1e-6f)
            return x;

        // Core cab shaping (wet)
        float y = x;

        // Remove rumble a bit before resonance (optional but helps)
        y = cabHP.process(y);

        // Speaker resonance bump
        y = cabRes.process(y);

        // Cone HF rolloff
        y = cabLP.process(y);

        // Subtle phase smear (small effect; too much gets weird)
        const float smearMix = 0.20f;
        float s = smear.process(y);
        y = y * (1.0f - smearMix) + s * smearMix;

        // Cab blend
        return x * (1.0f - cabAmt) + y * cabAmt;
    }

    float processPost(float x)
    {
        float y = x;
        if (postHpEnabled) y = postHP.process(y);
        if (postLpEnabled) y = postLP.process(y);
        return y;
    }

    // ---------- Smoother time helpers (for reset re-init) ----------
    float bassSTimeMs() const { return 20.0f; }
    float midSTimeMs() const { return 20.0f; }
    float trebleSTimeMs() const { return 20.0f; }
    float presenceSTimeMs() const { return 20.0f; }
    float cabAmtSTimeMs() const { return 25.0f; }
    float mixSTimeMs() const { return 10.0f; }
    float lowCutSTimeMs() const { return 30.0f; }
    float highCutSTimeMs() const { return 30.0f; }

private:
    float sr  = 48000.0f;
    float nyq = 24000.0f;

    ToneType toneType = ToneType::FENDER;
    CabType  cabType  = CabType::OPEN;

    // Parameters + smoothers
    Parameter  bassP, midP, trebleP, presenceP;
    Parameter  cabAmtP, mixP;
    Parameter  lowCutP, highCutP;

    ParamLinear bassS{0.5f}, midS{0.5f}, trebleS{0.5f}, presenceS{0.35f};
    ParamLinear cabAmtS{0.75f}, mixS{1.0f};
    ParamLinear lowCutS{80.0f}, highCutS{6500.0f};

    // DSP blocks
    SOS toneStack;               // 3 x Biquad sections
    LowPassFilter  cabLP;
    HighPassFilter cabHP;
    Reson          cabRes;
    AllPass2Block  smear;

    HighPassFilter postHP;
    LowPassFilter  postLP;

    bool postHpEnabled = true;
    bool postLpEnabled = true;

    // Dirty flags (coefficient updates)
    bool dirtyTone = true;
    bool dirtyCab  = true;
    bool dirtyPost = true;
};
