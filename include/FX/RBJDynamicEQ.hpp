#pragma once
#include <vector>
#include "RBJFilter.hpp"

template<class Sample>
class DynamicEQBand {
public:
    using Type = typename RBJFilter<Sample>::Type;

    enum DynMode     { CUT_ONLY, BOOST_ONLY, CUT_BOOST };
    enum StereoLink  { UNLINKED, MAX_LINK, RMS_LINK };
    enum StereoMode  { STEREO, MID, SIDE };

    DynamicEQBand(Sample sampleRate)
    : mSR(sampleRate),
      mFilter(sampleRate, Type::PEAKING),
      mDetector(sampleRate, Type::BPF_PEAK_0DB),
      mBypassMix(1.f),
      mTotalGain(0.f),
      mWidth(1.f),
      mAutoGain(0.f)
    {
        updateEnvCoeffs();
        updateDetectorShape();
    }

    // ---------------- Band Controls ----------------
    void enable(bool e) { mBypassMix.set(e ? 1.f : 0.f); }

    void setType(Type t) { mType = t; mFilter.setType(t); updateDetectorShape(); }

    void setFreq(Sample f) {
        mFreq = std::clamp(f, 1.f, 0.45f * mSR);
        mFilter.setFreq(mFreq);
        updateDetectorShape();
    }

    void setQ(Sample q) {
        mQ = std::max(0.001f, q);
        mFilter.setQ(mQ);
        updateDetectorShape();
    }

    void setGainDb(Sample db) { mBaseGainDb = db; }

    void setShelfSlope(Sample s) { mFilter.setShelfSlope(std::max(0.001f, s)); }

    // ---------------- Dynamic Controls ----------------
    void enableDynamic(bool e) { mDynEnabled = e; }

    void setDynMode(DynMode m) { mMode = m; }

    void setThresholdDb(Sample db) { mThresholdDb = db; }

    void setRatio(Sample r) { mRatio = std::max(1.f, r); }

    void setRangeDb(Sample db) { mRangeDb = std::max(0.f, db); }

    void setAttackMs(Sample ms)  { mAttackMs = std::max(0.1f, ms); updateEnvCoeffs(); }

    void setReleaseMs(Sample ms) { mReleaseMs = std::max(0.1f, ms); updateEnvCoeffs(); }

    // ---------------- Stereo / Width / AutoGain ----------------
    void setStereoLink(StereoLink m) { mLinkMode = m; }

    void setStereoMode(StereoMode m) { mStereoMode = m; }

    StereoMode stereoMode() const { return mStereoMode; }

    StereoLink stereoLink() const { return mLinkMode; }

    void enableAutoGain(bool e) { mAutoGainEnabled = e; }

    void setWidth(Sample w) { mWidth.set(std::clamp(w, 0.f, 3.f)); }

    Sample width() const { return mWidth.value(); }

    Sample lastTotalGainDb() const { return mLastTotalGainDb; }

    // ---------------- DSP ----------------
    Sample process(Sample x, Sample sidechain)
    {
        const Sample dry = x;

        // Detector
        Sample det = mDetector.process(sidechain);
        Sample mag = std::fabs(det);

        Sample coef = (mag > mEnv) ? aAtk : aRel;
        mEnv = coef * mEnv + (1.f - coef) * mag;
        Sample envDb = 20.f * std::log10(std::max(mEnv, 1e-12f));

        // Gain computer
        Sample targetDyn = 0.f;
        Sample over = envDb - mThresholdDb;

        if (mDynEnabled) {
            if (mMode != BOOST_ONLY && over > 0.f) {
                Sample gr = std::min(over - over / mRatio, mRangeDb);
                targetDyn -= gr;
            }
            if (mMode != CUT_ONLY && over < 0.f) {
                Sample up = std::min(-over + over / mRatio, mRangeDb);
                targetDyn += up;
            }
        }

        // Dynamic smoothing
        Sample gCoef = (targetDyn < mDynGainDb) ? aAtk : aRel;
        mDynGainDb = gCoef * mDynGainDb + (1.f - gCoef) * targetDyn;

        // Total gain into filter
        Sample totalGainDb = mBaseGainDb + mDynGainDb;
        mLastTotalGainDb = totalGainDb;

        mTotalGain.set(totalGainDb);
        mFilter.setGainDb(mTotalGain.process());

        // Filter audio
        Sample y = mFilter.process(x);

        // Auto-gain
        if (mAutoGainEnabled)
            mAutoGain.set(std::clamp(-0.6f * totalGainDb, -12.f, 12.f));
        else
            mAutoGain.set(0.f);

        y *= std::pow(10.f, mAutoGain.process() / 20.f);

        // Bypass
        Sample mix = mBypassMix.process();
        return dry + (y - dry) * mix;
    }

    void reset()
    {
        mFilter.reset();
        mDetector.reset();
        mEnv = 0.f;
        mDynGainDb = 0.f;
    }

private:
    void updateEnvCoeffs()
    {
        auto msToCoef = [&](Sample ms) {
            Sample t = std::max(0.001f, ms * 0.001f);
            return std::exp(-1.f / (t * mSR));
        };
        aAtk = msToCoef(mAttackMs);
        aRel = msToCoef(mReleaseMs);
    }

    void updateDetectorShape()
    {
        mDetector.setFreq(mFreq);
        mDetector.setQ(mQ);
    }

private:
    Sample mSR;

    RBJFilter<Sample> mFilter, mDetector;

    ParamLinear mBypassMix;
    ParamLinear mTotalGain;
    ParamLinear mWidth;
    ParamLinear mAutoGain;

    Type mType = Type::PEAKING;
    Sample mFreq = 1000.f;
    Sample mQ = 0.707f;

    Sample mBaseGainDb = 0.f;
    Sample mDynGainDb  = 0.f;
    Sample mLastTotalGainDb = 0.f;

    bool mDynEnabled = false;
    bool mAutoGainEnabled = false;

    DynMode mMode = CUT_ONLY;
    StereoLink mLinkMode = MAX_LINK;
    StereoMode mStereoMode = STEREO;

    Sample mThresholdDb = -24.f;
    Sample mRatio = 2.f;
    Sample mRangeDb = 12.f;

    Sample mAttackMs = 10.f;
    Sample mReleaseMs = 150.f;

    Sample mEnv = 0.f;
    Sample aAtk = 0.f;
    Sample aRel = 0.f;
};



template<class Sample>
class DynamicEQ : public Function {
public:
    DynamicEQ(Sample sampleRate, unsigned bands)
    : mBypassMix(1.f)
    {
        mBands.reserve(bands);
        for(unsigned i = 0; i < bands; ++i)
            mBands.emplace_back(sampleRate);

        // sensible defaults
        for(unsigned i = 0; i < bands; ++i) {
            auto& b = mBands[i];
            b.enable(true);
            b.enableDynamic(false);
            b.setType(RBJFilter::Type::PEAKING);
            b.setFreq(200.f * std::pow(2.f, Sample(i)));
            b.setQ(0.707f);
            b.setGainDb(0.f);
            b.setShelfSlope(1.f);
            b.setThresholdDb(-24.f);
            b.setRatio(2.f);
            b.setRangeDb(12.f);
            b.setAttackMs(10.f);
            b.setReleaseMs(150.f);
            b.setDynMode(DynamicEQBand::CUT_ONLY);
        }
    }

    // ---- Global ----
    void bypass(bool b) { mBypassMix.set(b ? 0.f : 1.f); }

    unsigned numBands() const { return (unsigned)mBands.size(); }
    DynamicEQBand<Sample>& band(unsigned i) { return mBands[i]; }

    // ---- DSP ----
    Sample process(Sample input) override
    {
        const Sample dry = input;

        Sample x = input;
        for(auto& b : mBands)
            x = b.process(x, input);

        Sample mix = mBypassMix.process();
        return dry + (x - dry) * mix;
    }

    void run(const Sample* in, Sample* out, size_t n) override
    {
        for(size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

    void reset()
    {
        for(auto& b : mBands)
            b.reset();
    }

private:
    std::vector<DynamicEQBand<Sample>> mBands;
    ParamLinear mBypassMix;
};

