#include <vector>
#include <algorithm>
#include "LorisTracks.hpp"

class AdditiveResynthTracks: public Generator
{
public:
    explicit AdditiveResynthTracks(float sampleRate)
    : mSampleRate(sampleRate),
      mDt(1.0 / sampleRate)
    {}

    // ------------------------------------------------------------
    // Load tracks (call once or when changing model)
    // ------------------------------------------------------------

    void setTracks(const std::vector<PartialTrack>& tracks)
    {
        mTracks = &tracks;

        mOsc.clear();
        mCursor.clear();

        for (const auto& t : tracks)
        {
            mOsc.emplace_back(mSampleRate);
            mCursor.push_back({ &t, 0 });
        }

        reset();
    }

    // ------------------------------------------------------------
    // GeneratorBlock interface
    // ------------------------------------------------------------

    void reset() override
    {
        mTime = 0.0;

        for (auto& c : mCursor)
            c.idx = 0;

        for (auto& o : mOsc)
            o.setPhase(0.0f);
    }

    float process() override
    {
        
        float y = 0.0f;

        for (size_t k = 0; k < mOsc.size(); ++k)
        {
            auto& cur = mCursor[k];

            if (!cur.active(mTime))
                continue;

            cur.advance(mTime);

            float freq, amp;
            cur.sample(mTime, freq, amp);

            if (amp > mAmpThreshold)
            {
                mOsc[k].setFreq(freq);
                mOsc[k].setAmp(amp * mAmpScale);
                y += mOsc[k].process();
            }
        }        
        mTime += mDt;
        return y;
    }

    // ------------------------------------------------------------
    // Controls
    // ------------------------------------------------------------

    void setAmpScale(float g)      { mAmpScale = g; }
    void setAmpThreshold(float t) { mAmpThreshold = t; }

    // Optional sequencing hooks
    void setTime(double t)        { mTime = t; }
    double time() const           { return mTime; }

private:
    const std::vector<PartialTrack>* mTracks = nullptr;

    float  mSampleRate;
    double mDt;
    double mTime = 0.0;

    std::vector<CSine>       mOsc;
    std::vector<TrackCursor> mCursor;

    float mAmpScale     = 1.0f;
    float mAmpThreshold = 1e-5f;
};
