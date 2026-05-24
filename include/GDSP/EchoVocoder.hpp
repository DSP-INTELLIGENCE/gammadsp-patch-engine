// EchoVocoder.hpp
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// Uses your existing wrappers:
//  - MultitapDelay (read(tap), write(x), setTapTime(tap, sec))
//  - BandPassFilter (freq(), res(), level(), process(x))
//  - EnvelopeFollower (attack/release)
//  - gam::sampleRate()

static inline float clampf(float x, float a, float b){ return std::min(std::max(x,a),b); }
static inline float onepole(float y, float x, float g){ return y + g * (x - y); }

//======================================================
// EchoAnalyzer (time-domain analysis filterbank)
//======================================================
class EchoAnalyzer {
public:
    EchoAnalyzer(float maxDelaySeconds,
                 int   numTaps,
                 float minDelaySeconds,
                 float maxDelayTapSeconds)
    : mDelay(maxDelaySeconds, (unsigned)std::max(1, numTaps)),
      mTap(std::max(1, numTaps), 0.0f),
      mEnv(std::max(1, numTaps), 0.0f),
      mDecay(std::max(1, numTaps), 1.0f),
      mEF(std::max(1, numTaps))
    {
        // Log spacing in time (good for “echo spectrum” perception)
        const int N = size();
        for (int i = 0; i < N; ++i)
        {
            float t = (N == 1) ? 0.0f : float(i) / float(N - 1);
            float d = minDelaySeconds * std::pow(maxDelayTapSeconds / std::max(1e-6f, minDelaySeconds), t);
            mDelay.setTapTime((unsigned)i, std::max(0.00001f, d));
        }

        setEnvelope(0.010f, 0.080f); // attack, release (seconds)
        setNormalize(true);
    }

    int size() const { return (int)mTap.size(); }

    void setTapDelay(int i, float seconds)
    {
        if (i < 0 || i >= size()) return;
        mDelay.setTapTime((unsigned)i, std::max(0.00001f, seconds));
    }

    void setEnvelope(float attackSec, float releaseSec)
    {
        float sr = (float)gam::sampleRate();
        for (auto& ef : mEF) ef.set(std::max(1e-5f, attackSec), std::max(1e-5f, releaseSec), sr);
    }

    void setNormalize(bool on) { mNormalize = on; }

    // Feed audio; analyzer taps are read from the delay line after write()
    inline void process(float x)
    {
        mDelay.write(x);

        float sum = 0.0f;
        const int N = size();

        for (int i = 0; i < N; ++i)
        {
            float v = mDelay.read((unsigned)i);
            mTap[i] = v;

            float e = mEF[i].process(v);          // envelope follower uses abs internally
            mEnv[i] = e;
            sum += e;
        }

        // Optional normalization (turns env[] into a distribution over time)
        if (mNormalize && sum > 1e-9f)
        {
            float inv = 1.0f / sum;
            for (int i = 0; i < N; ++i) mEnv[i] *= inv;
            sum = 1.0f;
        }

        // Simple “temporal slope” metric
        mDecay[0] = 1.0f;
        for (int i = 1; i < N; ++i)
        {
            float prev = mEnv[i - 1];
            float cur  = mEnv[i];
            mDecay[i]  = (prev > 1e-6f) ? (cur / prev) : 0.0f;
        }

        mEnergy = sum;
    }

    float tap(int i)   const { return mTap[(size_t)i]; }
    float env(int i)   const { return mEnv[(size_t)i]; }
    float decay(int i) const { return mDecay[(size_t)i]; }
    float energy()     const { return mEnergy; }

private:
    MultitapDelay mDelay;

    std::vector<float> mTap;
    std::vector<float> mEnv;
    std::vector<float> mDecay;

    std::vector<EnvelopeFollower> mEF;

    float mEnergy = 0.0f;
    bool  mNormalize = true;
};


//======================================================
// EchoVocoder (EchoAnalyzer + Filterbank Synth)
//======================================================
class EchoVocoder {
public:
    // “Bands” == analyzer taps == filterbank bands
    EchoVocoder(int   bands          = 12,
                float maxDelaySec    = 1.0f,
                float minTapDelaySec = 0.015f,
                float maxTapDelaySec = 0.45f,
                float minFreqHz      = 120.0f,
                float maxFreqHz      = 6800.0f)
    : mBands(std::max(1, bands)),
      mAnalyzer(maxDelaySec, mBands, minTapDelaySec, maxTapDelaySec),
      mFilters((size_t)mBands),
      mGainZ((size_t)mBands, 0.0f),
      mPanZ((size_t)mBands, 0.0f)
    {
        setBandRange(minFreqHz, maxFreqHz);
        setQ(6.0f);
        setMix(1.0f);
        setDrive(0.0f);
        setEnv(0.010f, 0.090f);
        setGainSmoothMs(10.0f);

        setEchoDepth(1.0f);      // how strongly echo-env drives bands
        setTilt(0.0f);           // -1..+1 (dark..bright)
        setWidth(0.0f);          // optional stereo widening (0 mono, 1 wide)
        setNormalize(true);
    }

    // ---------------- Controls ----------------

    void setNormalize(bool on) { mAnalyzer.setNormalize(on); }

    void setEnv(float attackSec, float releaseSec) { mAnalyzer.setEnvelope(attackSec, releaseSec); }

    void setTapDelay(int i, float seconds) { mAnalyzer.setTapDelay(i, seconds); }

    void setBandRange(float minHz, float maxHz)
    {
        mMinHz = std::max(5.0f, minHz);
        mMaxHz = std::max(mMinHz * 1.01f, maxHz);
        rebuildFrequencies();
    }

    // Vocoder “bandwidth”: higher Q = tighter, more robotic
    void setQ(float Q)
    {
        mQ = std::max(0.1f, Q);
        for (auto& f : mFilters) f.res(mQ);
    }

    // 0..1 equal-power mix (0 dry, 1 wet)
    void setMix(float mix) { mMix = clampf(mix, 0.f, 1.f); }

    // Echo envelope -> band gain depth
    void setEchoDepth(float d) { mDepth = std::max(0.0f, d); }

    // Spectral tilt applied across bands (-1 dark .. +1 bright)
    void setTilt(float t) { mTilt = clampf(t, -1.f, 1.f); }

    // Optional widening (0 mono sum, 1 exaggerated side)
    void setWidth(float w) { mWidth = clampf(w, 0.f, 1.f); }

    // Simple soft drive on wet output (0..1+)
    void setDrive(float d) { mDrive = std::max(0.0f, d); }

    // Gain smoothing (ms) for stable band envelopes
    void setGainSmoothMs(float ms)
    {
        float sr = (float)gam::sampleRate();
        float tau = std::max(0.0005f, ms * 0.001f);
        // g = 1 - exp(-1/(tau*sr))
        mGainG = 1.f - std::exp(-1.f / (tau * sr));
        mGainG = clampf(mGainG, 0.000001f, 0.5f);
    }

    // ---------------- Processing ----------------
    // Analyze `modulator` echoes, impose envelope onto `carrier` via filterbank.
    // Stereo out optional; if you only want mono, use processMono().
    inline void process(float modulator, float carrier, float& outL, float& outR)
    {
        // 1) Analyze modulator in echo-domain
        mAnalyzer.process(modulator);

        // 2) Filterbank synth on carrier
        float wetM = 0.0f;     // mid
        float wetS = 0.0f;     // side (optional widening)

        for (int i = 0; i < mBands; ++i)
        {
            // target gain from echo envelope
            float g = mAnalyzer.env(i) * mDepth;

            // tilt shaping (log-ish across bands)
            float x = (mBands == 1) ? 0.0f : float(i) / float(mBands - 1); // 0..1
            float tilt = std::pow(2.f, (x - 0.5f) * 2.f * mTilt);           // ~0.5..2
            g *= tilt;

            // smooth band gain
            mGainZ[(size_t)i] = onepole(mGainZ[(size_t)i], g, mGainG);
            float gain = mGainZ[(size_t)i];

            // carrier band
            float band = mFilters[(size_t)i].process(carrier);

            // optional per-band pseudo-random widening decorrelation (static)
            // (kept deterministic across runs)
            float pan = mPanZ[(size_t)i];
            if (!mPanInit)
            {
                // deterministic spread in [-1,1]
                float p = std::sin(12.9898f * (i + 1) + 78.233f) * 43758.5453f;
                p = p - std::floor(p);
                pan = (p * 2.f - 1.f);
                mPanZ[(size_t)i] = pan;
            }

            float v = band * gain;

            wetM += v;
            wetS += v * pan;
        }
        mPanInit = true;

        // Normalize wet sum by number of bands (prevent level build-up)
        float invN = 1.0f / std::sqrt(std::max(1, mBands));
        wetM *= invN;
        wetS *= invN;

        // Convert M/S to L/R with width
        float side = wetS * (0.5f * mWidth);
        float wetL = wetM + side;
        float wetR = wetM - side;

        // Optional gentle drive on wet
        if (mDrive > 0.0f)
        {
            wetL = softsat(wetL, mDrive);
            wetR = softsat(wetR, mDrive);
        }

        // Equal-power mix with dry carrier (mono carrier to both channels)
        float theta = mMix * float(M_PI_2);
        float dryG  = std::cos(theta);
        float wetG  = std::sin(theta);

        float dry = carrier;

        outL = dry * dryG + wetL * wetG;
        outR = dry * dryG + wetR * wetG;
    }

    inline float processMono(float modulator, float carrier)
    {
        float L, R;
        process(modulator, carrier, L, R);
        return 0.5f * (L + R);
    }

    // Expose analysis if you want to drive other systems
    const EchoAnalyzer& analyzer() const { return mAnalyzer; }

private:
    void rebuildFrequencies()
    {
        mFilters.clear();
        mFilters.resize((size_t)mBands);

        for (int i = 0; i < mBands; ++i)
        {
            float t = (mBands == 1) ? 0.0f : float(i) / float(mBands - 1);
            // Log spacing in frequency
            float f = mMinHz * std::pow(mMaxHz / mMinHz, t);

            BandPassFilter bp;
            bp.freq(f);
            bp.res(mQ);
            bp.level(1.0f);

            mFilters[(size_t)i] = bp;
        }
    }

    static inline float softsat(float x, float drive)
    {
        // drive 0..1+; mild tanh
        float k = 1.f + 6.f * drive;
        return std::tanh(k * x) / std::tanh(k);
    }

private:
    int mBands = 12;

    EchoAnalyzer mAnalyzer;
    std::vector<BandPassFilter> mFilters;

    std::vector<float> mGainZ; // smoothed band gains
    std::vector<float> mPanZ;  // deterministic per-band pan seed
    bool mPanInit = false;

    float mMinHz = 120.0f;
    float mMaxHz = 6800.0f;

    float mQ     = 6.0f;
    float mMix   = 1.0f;
    float mDepth = 1.0f;
    float mTilt  = 0.0f;
    float mWidth = 0.0f;
    float mDrive = 0.0f;

    float mGainG = 0.02f; // smoothing coefficient
};
