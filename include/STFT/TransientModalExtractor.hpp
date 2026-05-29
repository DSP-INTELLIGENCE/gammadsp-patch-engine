#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>

class TransientModalExtractor {
public:
    struct Mode {
        float freqHz = 0.0f;
        float amp    = 0.0f;
        float decay60 = 0.25f;
        float phase  = 0.0f;
    };

    TransientModalExtractor(unsigned win=1024,
                            unsigned hop=256,
                            gam::WindowType winType=gam::HANN,
                            unsigned maxModes=32)
    : mSTFT(win, hop, 0, winType, gam::MAG_FREQ, /*numAux=*/2)
    , mMaxModes(std::max(1u, maxModes))
    , mRng(1337)
    {
        mSTFT.inverseWindowing(false); // analysis-only
        const unsigned bins = mSTFT.numBins();
        mPrevMag.assign(bins, 0.0f);
        mFluxHist.assign(32, 0.0f);
        mFluxHistPos = 0;

        mModes.resize(mMaxModes);
        mActiveModes = 0;

        mModal.resize(mMaxModes); // gam::SineDs bank size
    }

    // -------- Controls --------
    void setSensitivity(float v) { mSensitivity = std::clamp(v, 0.0f, 1.0f); }
    void setMinFreq(float hz)    { mMinFreqHz = std::max(0.0f, hz); }
    void setMaxModes(unsigned n) { resizeModes(n); }
    void setDecayRange(float lo, float hi) {
        mDecayLo = std::max(0.01f, lo);
        mDecayHi = std::max(mDecayLo, hi);
    }
    void setRandomPhase(bool v) { mRandomPhase = v; }
    void setOutputGain(float g) { mGain = std::max(0.0f, g); }

    // If you want to audition in Python: output is *only* modal layer.
    // Returns true if a transient triggered new modes this hop.
    bool run(const float* input, float* output, std::size_t n)
    {
        // n expected == hop, but we’ll support any multiple of hop
        std::fill(output, output + n, 0.0f);

        bool triggered = false;

        for (std::size_t i = 0; i < n; ++i)
        {
            if (mSTFT(input[i]))
            {
                // frame ready -> analyze -> maybe trigger modes
                triggered |= analyzeFrameAndTrigger();
            }

            // modal synth runs at audio rate (per-sample)
            output[i] += float(mModal()) * mGain;
        }

        return triggered;
    }

    // Read-only access for UI/debug
    unsigned activeModes() const { return mActiveModes; }
    const Mode* modes() const { return mModes.data(); }

private:
    void resizeModes(unsigned n)
    {
        n = std::max(1u, n);
        mMaxModes = n;
        mModes.resize(mMaxModes);
        mModal.resize(mMaxModes);
        mActiveModes = 0;
    }

    bool analyzeFrameAndTrigger()
    {
        const unsigned bins = mSTFT.numBins();

        // pull mag/freq to aux buffers (Gamma guarantees aux size == numBins)
        mSTFT.copyBinsToAux(0, 0); // mag
        mSTFT.copyBinsToAux(1, 1); // freqHz
        float* mag   = mSTFT.aux(0);
        float* freqH = mSTFT.aux(1);

        // -------- Spectral flux transient detection --------
        float flux = 0.0f;
        float energy = 1e-12f;

        for (unsigned k = 1; k + 1 < bins; ++k)
        {
            const float m = mag[k];
            const float d = m - mPrevMag[k];
            if (d > 0.0f) flux += d;
            energy += m * m;
        }

        // normalize by energy to reduce level dependence
        flux /= std::sqrt(energy);

        // adaptive threshold (median/mean of small history + sensitivity factor)
        mFluxHist[mFluxHistPos] = flux;
        mFluxHistPos = (mFluxHistPos + 1) % mFluxHist.size();

        float mean = 0.0f;
        for (float v : mFluxHist) mean += v;
        mean /= float(mFluxHist.size());

        // sensitivity: higher => easier to trigger
        const float thresh = mean * (1.2f - 0.9f * mSensitivity);

        // update prev mag for next frame
        for (unsigned k = 0; k < bins; ++k) mPrevMag[k] = mag[k];

        if (flux <= thresh) return false;

        // -------- Peak picking --------
        pickModesFromSpectrum(mag, freqH, bins);

        // -------- Trigger modal bank --------
        for (unsigned i = 0; i < mActiveModes; ++i)
        {
            const auto& md = mModes[i];
            mModal.set(i, md.freqHz, md.amp, md.decay60, md.phase);
        }

        return true;
    }

    void pickModesFromSpectrum(const float* mag,
                               const float* freqH,
                               unsigned bins)
    {
        // gather candidate peaks
        mPeakIdx.clear();
        mPeakIdx.reserve(128);

        const float nyq = 0.5f * float(gam::sampleRate());
        const float minF = std::clamp(mMinFreqHz, 0.0f, nyq);

        for (unsigned k = 2; k + 2 < bins; ++k)
        {
            const float f = freqH[k];
            if (f < minF) continue;

            const float m = mag[k];
            if (m <= 0.0f) continue;

            // simple local maximum
            if (m > mag[k-1] && m > mag[k+1])
                mPeakIdx.push_back(k);
        }

        // sort peaks by magnitude descending
        std::sort(mPeakIdx.begin(), mPeakIdx.end(),
                  [&](unsigned a, unsigned b){ return mag[a] > mag[b]; });

        // select top-N with frequency masking
        mActiveModes = 0;
        const float minSepHz = 40.0f; // prevents clustered bins becoming many modes

        for (unsigned idx : mPeakIdx)
        {
            if (mActiveModes >= mMaxModes) break;

            const float f = freqH[idx];
            const float a = mag[idx];

            // reject tiny peaks relative to strongest
            if (mActiveModes == 0) mPeakTop = a;
            if (a < 0.10f * mPeakTop) break;

            bool tooClose = false;
            for (unsigned j = 0; j < mActiveModes; ++j)
            {
                if (std::fabs(mModes[j].freqHz - f) < minSepHz) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            Mode md;
            md.freqHz = f;
            md.amp    = a * 0.5f;     // scale to taste (STFT mag isn't “direct” amp)
            md.decay60 = decayForFreq(f);
            md.phase  = mRandomPhase ? randPhase() : 0.0f;

            mModes[mActiveModes++] = md;
        }

        // zero unused modes (optional)
        for (unsigned i = mActiveModes; i < mMaxModes; ++i)
            mModes[i] = Mode{};
    }

    float decayForFreq(float fHz) const
    {
        // simple musical mapping: lows ring longer than highs
        const float nyq = 0.5f * float(gam::sampleRate());
        float x = std::clamp(fHz / std::max(1.0f, nyq), 0.0f, 1.0f);
        // curve: longer at low freq
        float t = (1.0f - x);
        t = t * t; // emphasize lows
        return mDecayLo + (mDecayHi - mDecayLo) * t;
    }

    float randPhase()
    {
        std::uniform_real_distribution<float> dist(0.0f, 2.0f * 3.1415926535f);
        return dist(mRng);
    }

private:
    gam::STFT mSTFT;

    // transient detection state
    std::vector<float> mPrevMag;
    std::vector<float> mFluxHist;
    std::size_t mFluxHistPos = 0;

    // peak selection scratch
    std::vector<unsigned> mPeakIdx;
    float mPeakTop = 0.0f;

    // modal output
    unsigned mMaxModes = 32;
    unsigned mActiveModes = 0;
    std::vector<Mode> mModes;

    gam::SineDs<double> mModal;  // modal bank

    // controls
    float mSensitivity = 0.5f;
    float mMinFreqHz = 60.0f;
    float mDecayLo = 0.08f;
    float mDecayHi = 0.80f;
    bool  mRandomPhase = true;
    float mGain = 1.0f;

    std::mt19937 mRng;
};
