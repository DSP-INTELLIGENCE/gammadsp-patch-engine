#pragma once

#include <algorithm>
#include "LorisTracks.hpp"
#include "AdditiveResynthTracks.hpp"

// ============================================================
// Tracker
// - Owns TrackModel
// - Controls transport (time / play / stop / seek)
// - Drives AdditiveResynthTracks
// ============================================================

class Tracker
{
public:
    explicit Tracker(float sampleRate)
    : mSampleRate(sampleRate),
      mDt(1.0 / sampleRate),
      mSynth(sampleRate)
    {}

    // ------------------------------------------------------------
    // Load analysis file
    // ------------------------------------------------------------

    bool load(const char* path)
    {
        TrackModel model;
        if (!loadTrackModelJson(path, model))
            return false;

        if (model.sampleRate <= 0.0 || model.tracks.empty())
            return false;

        mModel = std::move(model);
        mSynth.setTracks(mModel.tracks);
        reset();
        return true;
    }

    // ------------------------------------------------------------
    // Transport
    // ------------------------------------------------------------

    void play()  { mPlaying = true;  }
    void stop()  { mPlaying = false; }

    void reset()
    {
        mTime = 0.0;
        mSynth.reset();
    }

    void seek(double t)
    {
        mTime = t;
        mSynth.reset();
        mSynth.setTime(t);
    }

    bool isPlaying() const { return mPlaying; }

    // ------------------------------------------------------------
    // Audio generation
    // ------------------------------------------------------------

    float process()
    {
        if (!mPlaying)
            return 0.0f;

        float y = mSynth.process();
        mTime += mDt;

        if (mTime > endTime())
            mPlaying = false;

        return y;
    }

    void processBlock(float* out, unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            out[i] = process();
    }

    // ------------------------------------------------------------
    // Controls
    // ------------------------------------------------------------

    void setAmpScale(float g)      { mSynth.setAmpScale(g); }
    void setAmpThreshold(float t) { mSynth.setAmpThreshold(t); }

    double time() const { return mTime; }

    // ------------------------------------------------------------
    // Info
    // ------------------------------------------------------------

    double endTime() const
    {
        double t = 0.0;
        for (const auto& tr : mModel.tracks)
            t = std::max(t, tr.endTime);
        return t;
    }

    size_t trackCount() const
    {
        return mModel.tracks.size();
    }

private:
    float  mSampleRate;
    double mDt;

    bool   mPlaying = false;
    double mTime    = 0.0;

    TrackModel             mModel;
    AdditiveResynthTracks  mSynth;
};
