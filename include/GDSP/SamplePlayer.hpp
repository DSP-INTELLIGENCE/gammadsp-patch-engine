// SamplePlayer.hpp
#pragma once

#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "Engine.hpp"
#include "Parameters.hpp"

//////////////////////////////////////////////////////////
// SamplePlayer wrapper (GDSP)
// - Generator: outputs mono (selectable channel / mixdown)
// - RT-safe: process(), run(), setRate/pos/min/max/loop/trigger
// - NOT RT-safe: load(), setBuffer() that reallocates / changes pointers
//////////////////////////////////////////////////////////

class SamplePlayer : public Generator {
public:
    enum Mode {
        ONE_SHOT = 0,
        LOOP
    };

    enum OutputMode {
        OUT_CH0 = 0,     // output channel 0
        OUT_CH1,         // output channel 1
        OUT_MIXDOWN,     // average all channels
        OUT_STEREO_MIX_L // (for convenience) average to "left-like" (same as mixdown for N>1)
    };

    // Interpolation: choose at compile time by changing Ipl template.
    // Default here uses Linear for smooth pitch/rate modulation.
    using PlayerT = gam::SamplePlayer<float, gam::ipl::Linear, gam::phsInc::OneShot, gam::Domain1>;

public:
    explicit SamplePlayer()    
    {        
        resetState_();
    }

    ~SamplePlayer() override = default;

    // -----------------------------
    // Non-RT operations (DON'T call on audio thread)
    // -----------------------------

    bool load(const char* path) {
        bool ok = mPlayer.load(path);
        if (ok) {
            // Keep default interval to whole sample
            mPlayer.min(0);
            mPlayer.max(mPlayer.frames());
            resetState_();
        }
        return ok;
    }

    // Reference external buffer (must remain valid)
    // If interleaved=false, buffer is planar: [ch0 frames][ch1 frames]...
    // If interleaved=true, buffer is tightly interleaved: LRLRLR...
    void setBuffer(float* samples,
                   int frames,
                   double sourceFrameRate,
                   int channels,
                   bool interleaved)
    {
        mPlayer.buffer(samples, frames, sourceFrameRate, channels, interleaved);
        mPlayer.min(0);
        mPlayer.max(mPlayer.frames());
        resetState_();
    }

    void setBuffer(gam::Array<float>& src,
                   double sourceFrameRate,
                   int channels,
                   bool interleaved)
    {
        mPlayer.buffer(src, sourceFrameRate, channels, interleaved);
        mPlayer.min(0);
        mPlayer.max(mPlayer.frames());
        resetState_();
    }

    // -----------------------------
    // RT-safe controls
    // -----------------------------

    void setMode(Mode m) { mMode = m; }

    void setOutputMode(OutputMode m) { mOutMode = m; }

    void setChannel(int ch) { mChannel = std::max(0, ch); }

    // Playback window in frames
    void setMinFrame(double v) { mMin = v; mPlayer.min(v); }
    void setMaxFrame(double v) { mMax = v; mPlayer.max(v); }

    // Position in frames (absolute)
    void setPosFrames(double v) { mPos = v; mPlayer.pos(v); }

    // Position normalized in [0,1) within WHOLE buffer
    void setPhase(double ph) { mPos = ph * (double)mPlayer.frames(); mPlayer.phase(ph); }

    // Playback rate scalar. 1.0 = original speed.
    void setRate(double r) { mRate = r; mPlayer.rate(r); }

    // Gain applied after reading
    void setGain(float g) { mGain = std::max(0.f, g); }

    // One-shot trigger: resets head to min (or max if reversed)
    void trigger() {
        mPlayer.reset();
        mTriggered = true;
    }

    // Stop: move head to end so done()==true for one-shots.
    void stop() {
        mPlayer.finish();
    }

    // Query
    bool done() const { return mPlayer.done(); }
    int  channels() const { return mPlayer.channels(); }
    int  frames() const { return mPlayer.frames(); }
    double pos() const { return mPlayer.pos(); }
    
    // If true, pPos is interpreted as normalized phase [0..1).
    void setPosParamNormalized(bool v) { mPosParamNormalized = v; }

    // -----------------------------
    // Audio
    // -----------------------------
    float process() override {
        if (!hasData_()) return 0.f;

        // Apply modulated params
        applyParams_();

        // Read sample(s)
        float y = renderOutput_();

        // Advance head (SamplePlayer::operator() advances; read() doesn't)
        // We used read() for mixdown; so advance once manually:
        mPlayer.advance();

        // Looping behavior is runtime-switchable via loop()
        if (mMode == LOOP) {
            (void)mPlayer.loop();
        }

        // Hard gate when one-shot is finished (prevents denorm tails in some graphs)
        if (mMode == ONE_SHOT && mPlayer.done()) {
            return 0.f;
        }

        return y * mGain;
    }

    void run(float* out, size_t n) {
        for (size_t i = 0; i < n; ++i) out[i] = process();
    }

private:
    float renderOutput_() const {
        const int chs = std::max(1, mPlayer.channels());

        switch (mOutMode) {
            case OUT_CH0:
                return mPlayer.read(0);

            case OUT_CH1:
                return mPlayer.read(std::min(1, chs - 1));

            case OUT_MIXDOWN: {
                float sum = 0.f;
                for (int c = 0; c < chs; ++c) sum += mPlayer.read(c);
                return sum / (float)chs;
            }

            case OUT_STEREO_MIX_L: {
                // If stereo: take left; if >2: mixdown.
                if (chs >= 2) return mPlayer.read(0);
                return mPlayer.read(0);
            }

            default:
                break;
        }

        // Fallback: selected channel
        return mPlayer.read(std::min(mChannel, chs - 1));
    }

    void applyParams_() {
        // Rate
        if (pRate) {
            // Pattern used in your code: p->set(current) then p->process()
            pRate->set((float)mRate);
            double r = (double)pRate->process();
            // Allow reverse; clamp insane rates
            r = std::clamp(r, -64.0, 64.0);
            if (r != mRate) {
                mRate = r;
                mPlayer.rate(mRate);
            }
        }

        // Position (frames or normalized)
        if (pPos) {
            if (mPosParamNormalized) {
                pPos->set(0.f);
                double ph = (double)pPos->process();
                ph = ph - std::floor(ph); // wrap
                mPlayer.phase(ph);
            } else {
                pPos->set((float)mPos);
                double fr = (double)pPos->process();
                // Safe clip to interval
                fr = std::clamp(fr, mPlayer.min(), mPlayer.max());
                mPos = fr;
                mPlayer.pos(mPos);
            }
        }

        // Gain
        if (pGain) {
            pGain->set(mGain);
            float g = pGain->process();
            mGain = std::max(0.f, g);
        }
    }

    bool hasData_() const {
        // Array<T>::size() returns total samples, not frames
        return mPlayer.size() > 0 && mPlayer.frames() > 0 && mPlayer.channels() > 0;
    }

    void resetState_() {
        mMode = ONE_SHOT;
        mOutMode = OUT_CH0;
        mChannel = 0;

        mGain = 1.f;
        mRate = 1.0;
        mPos  = 0.0;

        mMin = 0.0;
        mMax = 1.0;

        mPosParamNormalized = false;

        mTriggered = false;

        // Reset head for predictable start
        mPlayer.reset();
    }

private:
    float mSR = 44100.f;

    PlayerT mPlayer;

    // playback
    Mode mMode = ONE_SHOT;
    OutputMode mOutMode = OUT_CH0;
    int mChannel = 0;

    float  mGain = 1.f;
    double mRate = 1.0;
    double mPos  = 0.0;
    double mMin  = 0.0;
    double mMax  = 1.0;

    bool mPosParamNormalized = false;
    bool mTriggered = false;    
};