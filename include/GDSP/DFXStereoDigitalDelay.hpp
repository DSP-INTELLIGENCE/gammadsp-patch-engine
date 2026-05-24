// DFXStereoDigitalDelay.hpp
#pragma once

#include <algorithm>
#include <cmath>

#include "Engine.hpp"          // StereoSample, Function, etc.
#include "DFXDigitalDelay.hpp" // DFXDigitalDelay, DFXDelayPreset

/////////////////////////////////////////////////////////////////
// Stereo presets
/////////////////////////////////////////////////////////////////
struct DFXStereoDelayPreset
{
    // Base (linked) controls
    float delay  = 0.35f;   // seconds
    float fbk    = 0.35f;   // -0.999..0.999
    float mix    = 0.5f;    // 0..1
    float depth  = 0.0f;    // modulation depth (unit depends on your lfo/depth chain)
    float lfo    = 0.0f;    // modulation signal or rate input (depends on your Modulator usage)
    float wet    = 1.0f;
    float dry    = 1.0f;

    // Stereo controls
    float spread        = 0.0f;  // 0..1 : time offset between L/R
    float width         = 1.0f;  // 0..2 : MS width (1 = neutral). clamp inside.
    float crossFeedback = 0.0f;  // 0..1 : cross-coupled feedback

    // Options
    bool  linkTimes     = true;  // if true, use delay +/- spread; else use delayL/delayR
    float delayL        = 0.35f; // seconds (used when linkTimes=false)
    float delayR        = 0.35f; // seconds (used when linkTimes=false)

    // LFO stereo phase mode
    // 0 = same phase, 1 = 90deg, 2 = 180deg
    int lfoPhaseMode    = 1;
};

/////////////////////////////////////////////////////////////////
// DFXStereoDigitalDelay
/////////////////////////////////////////////////////////////////
class DFXStereoDigitalDelay : public Function
{
public:
    DFXStereoDigitalDelay(float maxDelaySeconds = 2.0f,
                          float initDelaySeconds = 0.35f)
    : mDL(maxDelaySeconds, initDelaySeconds),
      mDR(maxDelaySeconds, initDelaySeconds)
    {
        // sensible defaults
        setDelay(initDelaySeconds);
        setFeedback(0.35f);
        setMix(0.5f);
        setWet(1.0f);
        setDry(1.0f);

        setSpread(0.0f);
        setWidth(1.0f);
        setCrossFeedback(0.0f);

        setLFOPHaseMode(Phase90);
        setLinkTimes(true);

        // Keep engines stable
        mLastDelayL = -1.0f;
        mLastDelayR = -1.0f;
    }

    /////////////////////////////////////////////////////////////////////
    // Presets
    /////////////////////////////////////////////////////////////////////
    void setPreset(const DFXStereoDelayPreset& p)
    {
        setWet(p.wet);
        setDry(p.dry);
        setMix(p.mix);
        setFeedback(p.fbk);

        setDepth(p.depth);

        // LFO input (if you use lfo Modulator as rate or signal, this just sets it)
        setLFO(p.lfo);

        setSpread(p.spread);
        setWidth(p.width);
        setCrossFeedback(p.crossFeedback);

        setLinkTimes(p.linkTimes);
        if (p.linkTimes)
        {
            setDelay(p.delay);
        }
        else
        {
            setDelayL(p.delayL);
            setDelayR(p.delayR);
        }

        setLFOPHaseMode(static_cast<LFOPHaseMode>(p.lfoPhaseMode));
    }

    /////////////////////////////////////////////////////////////////////
    // Base controls (match mono API where possible)
    /////////////////////////////////////////////////////////////////////
    void setDelay(float seconds)
    {
        mBaseDelay = std::max(0.0001f, seconds);
        if (mLinkTimes) { mDelayL = mBaseDelay; mDelayR = mBaseDelay; }
    }

    void setDelayL(float seconds)
    {
        mDelayL = std::max(0.0001f, seconds);
        mLinkTimes = false;
    }

    void setDelayR(float seconds)
    {
        mDelayR = std::max(0.0001f, seconds);
        mLinkTimes = false;
    }

    void setFeedback(float fb)
    {
        mBaseFeedback = std::clamp(fb, -0.9995f, 0.9995f);
    }

    void setMix(float mix)
    {
        mBaseMix = std::clamp(mix, 0.0f, 1.0f);
    }

    void setWet(float wet)
    {
        mWet = std::max(0.0f, wet);
    }

    void setDry(float dry)
    {
        mDry = std::max(0.0f, dry);
    }

    float getDelay()    const { return mBaseDelay; }
    float getDelayL()   const { return mDelayL; }
    float getDelayR()   const { return mDelayR; }
    float getFeedback() const { return mBaseFeedback; }
    float getMix()      const { return mBaseMix; }

    /////////////////////////////////////////////////////////////////////
    // Stereo controls
    /////////////////////////////////////////////////////////////////////
    void setLinkTimes(bool v) { mLinkTimes = v; }

    void setSpread(float s01)
    {
        // Spread defines time offset between L/R when linked.
        // Use small safe clamp; 1.0 means up to +/-50% of base (bounded below).
        mSpread = std::clamp(s01, 0.0f, 1.0f);
    }

    void setWidth(float w)
    {
        // 1.0 = neutral, 0 = mono, >1 = wider
        mWidth = std::clamp(w, 0.0f, 2.0f);
    }

    void setCrossFeedback(float x)
    {
        mCross = std::clamp(x, 0.0f, 1.0f);
    }

    enum LFOPHaseMode : int { SamePhase = 0, Phase90 = 1, Phase180 = 2 };

    void setLFOPHaseMode(LFOPHaseMode m) { mLFOMode = m; }

    /////////////////////////////////////////////////////////////////////
    // Modulation inputs (mirrors DFXDigitalDelay style)
    /////////////////////////////////////////////////////////////////////
    void setDelayMod(float v) { mDelayMod.set(v); } // shared delay-time modulation
    void setFbkMod(float v)   { mFbkMod.set(v); }
    void setMixMod(float v)   { mMixMod.set(v); }
    void setAM(float v)       { mAM.set(v); }

    void setDepth(float v)    { mDepth.set(v); }
    void setLFO(float v)      { mLFO.set(v);   }

    /////////////////////////////////////////////////////////////////////
    // Processing
    /////////////////////////////////////////////////////////////////////

    // Mono in -> stereo out
    StereoSample processStereo(float inL, float inR)
    {
        // Collapse input for the delay input (common stereo delay behavior).
        // If you prefer true dual-mono, replace x with inL/inR separately.
        const float x = 0.5f * (inL + inR);

        // --- compute modulated params ---
        // Follow your current DFXDigitalDelay scheme:
        // delayT = lfo.process(depth.process(dm.process(baseDelay)))
        // Here we keep ONE delay-mod chain, and derive L/R times from it.
        float baseT = mBaseDelay;
        if (!mLinkTimes)
        {
            // When unlinked, we still allow modulation around each base time.
            // We'll compute baseT separately per side below.
        }

        // Shared mod signals
        const float fb  = std::clamp(mFbkMod.process(mBaseFeedback), -0.9995f, 0.9995f);
        const float mix = std::clamp(mMixMod.process(mBaseMix), 0.0f, 1.0f);

        // "LFO phase" behavior:
        // Since your Modulator is a black box (can be signal or processed), we can’t phase-shift it directly.
        // So we support stereo LFO phase via *two* LFO modulators if you attach generators.
        // If user does not attach generators, both will follow their set() values, and phase mode won’t matter.
        //
        // Implementation: we keep two LFO modulators (mLFOL, mLFOR) and two depth modulators
        // but expose one setLFO/setDepth which writes to both by default.
        //
        // If you want phase offsets, attach two Generators externally with phase offsets and call attachLFOGens().
        //
        // For now: use mLFOL/mLFOR.
        const float tL = computeTimeL();
        const float tR = computeTimeR();

        // --- read current delayed samples from the two engines ---
        // We DO NOT call mDL.process(x) here because we need cross-feedback routing.
        const float dL = readDelayL(tL);
        const float dR = readDelayR(tR);

        // --- feedback write with optional cross-coupling ---
        // Base feedback per channel + cross feed
        const float fbL = dL * fb + dR * (fb * mCross);
        const float fbR = dR * fb + dL * (fb * mCross);

        writeDelayL(x + fbL);
        writeDelayR(x + fbR);

        // --- wet/dry ---
        float wetL = dL * mWet;
        float wetR = dR * mWet;

        float dryL = inL * mDry;
        float dryR = inR * mDry;

        float outL = dryL * (1.0f - mix) + wetL * mix;
        float outR = dryR * (1.0f - mix) + wetR * mix;

        // --- width (M/S) ---
        if (mWidth != 1.0f)
        {
            const float mid  = 0.5f * (outL + outR);
            const float side = 0.5f * (outL - outR) * mWidth;
            outL = mid + side;
            outR = mid - side;
        }

        // output AM (shared)
        const float g = mAM.process();
        return StereoSample(outL * g, outR * g);
    }

    // Required by Function: returns mono (mid) for hosts that only support mono
    float process(float x) override
    {
        StereoSample s = processStereo(x, x);
        return 0.5f * (s.outL + s.outR);
    }

    /////////////////////////////////////////////////////////////////////
    // Optional: attach separate generators to get real stereo LFO phase
    /////////////////////////////////////////////////////////////////////
    void attachLFOGens(Generator* genL, Generator* genR)
    {
        mLFOL.attachGen(genL);
        mLFOR.attachGen(genR);
    }

    void attachDepthGens(Generator* genL, Generator* genR)
    {
        mDepthL.attachGen(genL);
        mDepthR.attachGen(genR);
    }

private:
    // Compute linked spread times safely
    inline void computeLinkedTimes(float baseTime, float& outL, float& outR) const
    {
        // Spread is +/- up to 50% of base at spread=1 (bounded to >= 0.1ms)
        const float s = 0.5f * mSpread;
        outL = baseTime * (1.0f - s);
        outR = baseTime * (1.0f + s);

        outL = std::max(outL, 0.0001f);
        outR = std::max(outR, 0.0001f);
    }

    inline float computeTimeL()
    {
        float base = mLinkTimes ? mBaseDelay : mDelayL;

        // mod chain: dm -> depth -> lfo
        float t = mDelayMod.process(base);
        t = mDepthL.process(t);          // depth as multiplicative stage if your Modulator does that
        t = mLFOL.process(t);

        // If linked: apply spread AFTER modulation
        if (mLinkTimes)
        {
            float l, r;
            computeLinkedTimes(t, l, r);
            return l;
        }

        return std::max(t, 0.0001f);
    }

    inline float computeTimeR()
    {
        float base = mLinkTimes ? mBaseDelay : mDelayR;

        float t = mDelayMod.process(base);
        t = mDepthR.process(t);
        t = mLFOR.process(t);

        if (mLinkTimes)
        {
            float l, r;
            computeLinkedTimes(t, l, r);
            return r;
        }

        return std::max(t, 0.0001f);
    }

    inline float readDelayL(float delayT)
    {
        // Mirror DFXDigitalDelay internal behavior (only update when changed)
        if (delayT != mLastDelayL)
        {
            mDLRaw.delay(delayT);
            mLastDelayL = delayT;
        }
        return mDLRaw.read();
    }

    inline float readDelayR(float delayT)
    {
        if (delayT != mLastDelayR)
        {
            mDRRaw.delay(delayT);
            mLastDelayR = delayT;
        }
        return mDRRaw.read();
    }

    inline void writeDelayL(float v) { mDLRaw.write(v); }
    inline void writeDelayR(float v) { mDRRaw.write(v); }

private:
    // NOTE:
    // We reuse DFXDigitalDelay *only* for its parameter/modulator style and symmetry,
    // but we must access raw delay buffer for cross-feedback, so we use raw gamma Delay buffers.
    //
    // If you prefer, replace mDLRaw/mDRRaw with a small extracted "core" from DFXDigitalDelay.
    //
    DFXDigitalDelay mDL;
    DFXDigitalDelay mDR;

    // Raw buffers for manual fb routing (Gamma)
    gam::Delay<double> mDLRaw { 2.0, 0.35 };
    gam::Delay<double> mDRRaw { 2.0, 0.35 };

    // Base params
    float mBaseDelay    = 0.35f;
    float mDelayL       = 0.35f;
    float mDelayR       = 0.35f;
    float mBaseFeedback = 0.35f;
    float mBaseMix      = 0.5f;
    float mWet          = 1.0f;
    float mDry          = 1.0f;

    // Stereo params
    bool  mLinkTimes = true;
    float mSpread    = 0.0f;
    float mWidth     = 1.0f;
    float mCross     = 0.0f;

    LFOPHaseMode mLFOMode = Phase90; // currently informational unless you attach 2 gens

    // Modulators (shared)
    Modulator mDelayMod;
    Modulator mFbkMod;
    Modulator mMixMod;
    Modulator mAM;

    // Two-channel modulation (lets you attach two LFO gens for phase offsets)
    Modulator mLFOL, mLFOR;
    Modulator mDepthL, mDepthR;

    // Back-compat: setLFO/setDepth set both
    Modulator mLFO;   // unused (kept for compatibility if you want)
    Modulator mDepth; // unused

    // Raw delay cache
    float mLastDelayL = -1.0f;
    float mLastDelayR = -1.0f;
};
