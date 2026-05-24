// DynamicMidSideEQ.hpp
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "DynamicEQBand.hpp"

// ============================================================
// Dynamic Mid/Side EQ (multi-band, per-band routing/width, stereo-link)
// ============================================================

template<class Sample>
class DynamicMidSideEQ {
public:
    DynamicMidSideEQ(Sample sampleRate, unsigned bands)
    : mSR(sampleRate)
    {
        mBands.reserve(bands);
        for (unsigned i = 0; i < bands; ++i)
            mBands.emplace_back(sampleRate);

        // Sensible defaults
        for (unsigned i = 0; i < bands; ++i) {
            auto& b = mBands[i];
            b.enable(true);
            b.enableDynamic(false);
            b.enableAutoGain(false);

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

            b.setStereoMode(DynamicEQBand::STEREO);
            b.setStereoLink(DynamicEQBand::MAX_LINK);
            b.setWidth(1.f);
        }
    }

    unsigned numBands() const { return (unsigned)mBands.size(); }
    DynamicEQBand& band(unsigned i) { return mBands[i]; }
    const DynamicEQBand& band(unsigned i) const { return mBands[i]; }

    // Global controls (all smoothed)
    void bypass(bool b) { mBypassMix.set(b ? 0.f : 1.f); }

    void setInputTrimDb(Sample db)  { mInTrimDb.set(db); }
    void setOutputTrimDb(Sample db) { mOutTrimDb.set(db); }

    void enableGlobalAutoGain(bool e) { mGlobalAutoGainEnabled = e; }
    void setGlobalAutoGainAmount(Sample amt01) { mGlobalAutoGainAmt = std::clamp(amt01, 0.f, 1.f); }

    // DSP
    void process(Sample& L, Sample& R)
    {
        const Sample dryL = L, dryR = R;

        const Sample inG  = db_to_lin(mInTrimDb.process());
        const Sample outG = db_to_lin(mOutTrimDb.process());

        Sample xL = L * inG;
        Sample xR = R * inG;

        // Encode M/S once at start
        Sample M = encodeM(xL, xR);
        Sample S = encodeS(xL, xR);

        for (auto& b : mBands)
        {
            switch (b.stereoMode())
            {
            case DynamicEQBand::STEREO:
            {
                // Choose detector signals
                const Sample detL = xL;
                const Sample detR = xR;

                Sample sc = 0.f;
                switch (b.stereoLink())
                {
                case DynamicEQBand::UNLINKED:
                    // handled per-channel below
                    break;

                case DynamicEQBand::RMS_LINK:
                    sc = std::sqrt(0.5f * (detL * detL + detR * detR));
                    break;

                case DynamicEQBand::MAX_LINK:
                default:
                    sc = std::max(std::abs(detL), std::abs(detR));
                    break;
                }

                if (b.stereoLink() == DynamicEQBand::UNLINKED)
                {
                    xL = b.process(xL, detL);
                    xR = b.process(xR, detR);
                }
                else
                {
                    xL = b.process(xL, sc);
                    xR = b.process(xR, sc);
                }

                // Refresh M/S for downstream MID/SIDE bands
                M = encodeM(xL, xR);
                S = encodeS(xL, xR);

                // Per-band width always acts on SIDE
                S *= b.width();
                decodeMS(M, S, xL, xR);
                break;
            }

            case DynamicEQBand::MID:
                M = b.process(M, M);
                S *= b.width();
                decodeMS(M, S, xL, xR);
                break;

            case DynamicEQBand::SIDE:
                S = b.process(S, S);
                S *= b.width();
                decodeMS(M, S, xL, xR);
                break;
            }
        }

        // Optional global autogain (very light “feel-good” compensation)
        if (mGlobalAutoGainEnabled)
        {
            Sample loudnessEstimateDb = 0.f;
            for (auto& b : mBands)
                loudnessEstimateDb += b.lastTotalGainDb(); // already base+dyn

            // Normalize by number of bands and apply a conservative scaling
            loudnessEstimateDb /= std::max(1u, (unsigned)mBands.size());

            // Amount control (0..1), default 0.5 feels natural
            const Sample targetDb = -0.5f * loudnessEstimateDb * mGlobalAutoGainAmt;

            mGlobalAutoGainDb.set(std::clamp(targetDb, -12.f, 12.f));
        }
        else
        {
            mGlobalAutoGainDb.set(0.f);
        }

        const Sample globalComp = db_to_lin(mGlobalAutoGainDb.process());
        xL *= globalComp;
        xR *= globalComp;

        // Output trim
        xL *= outG;
        xR *= outG;

        // Global bypass crossfade
        const Sample mix = mBypassMix.process();
        L = dryL + (xL - dryL) * mix;
        R = dryR + (xR - dryR) * mix;
    }

    void run(const Sample* inL, const Sample* inR, Sample* outL, Sample* outR, size_t n)
    {
        for (size_t i = 0; i < n; ++i) {
            Sample L = inL[i];
            Sample R = inR[i];
            process(L, R);
            outL[i] = L;
            outR[i] = R;
        }
    }

    void reset()
    {
        for (auto& b : mBands) b.reset();
    }

private:
    static inline Sample db_to_lin(Sample db) { return std::pow(10.f, db / 20.f); }

    static constexpr Sample kInvSqrt2 = 0.7071067811865475f;

    static inline Sample encodeM(Sample L, Sample R) { return (L + R) * kInvSqrt2; }
    static inline Sample encodeS(Sample L, Sample R) { return (L - R) * kInvSqrt2; }

    static inline void decodeMS(Sample M, Sample S, Sample& L, Sample& R)
    {
        L = (M + S) * kInvSqrt2;
        R = (M - S) * kInvSqrt2;
    }

private:
    Sample mSR;
    std::vector<DynamicEQBand> mBands;

    // Globals (smoothed)
    ParamLinear mBypassMix       { 1.f };
    ParamLinear mInTrimDb        { 0.f };
    ParamLinear mOutTrimDb       { 0.f };

    // Optional global autogain
    bool       mGlobalAutoGainEnabled = false;
    Sample      mGlobalAutoGainAmt = 0.5f;      // 0..1
    ParamLinear mGlobalAutoGainDb { 0.f };
};
