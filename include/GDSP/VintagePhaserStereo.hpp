// VintagePhaserStereo.hpp
#pragma once
#include <cmath>
#include <algorithm>

static inline float clampf(float x, float a, float b){ return std::min(std::max(x,a),b); }
static inline float softsat(float x, float drive){
    // drive 0..1+
    if(drive <= 0.f) return x;
    float k = 1.f + 6.f * drive;
    return std::tanh(k * x) / std::tanh(k);
}

// Cheap band-limited drift noise (xorshift + onepole)
struct DriftNoise {
    uint32_t s = 0x12345678u;
    float y = 0.f;
    float g = 0.001f;
    void setRateHz(float hz, float sr){
        float x = 2.f * float(M_PI) * hz / sr;
        g = 1.f - std::exp(-x);
        g = clampf(g, 0.000001f, 0.5f);
    }
    inline float next(){
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        float n = (float(int32_t(s)) / 2147483648.0f); // [-1,1]
        y += g * (n - y);
        return y;
    }
};

// 1st-order all-pass (the building block of vintage phasers)
struct Allpass1 {
    // y[n] = -a x[n] + x[n-1] + a y[n-1]
    float a = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    void reset(){ x1 = y1 = 0.0f; }

    // Map center frequency (Hz) to coefficient a
    // a = (1 - tan(w/2)) / (1 + tan(w/2))
    void setFreq(float fc, float sr){
        fc = clampf(fc, 10.0f, 0.45f * sr);
        float w = float(M_PI) * (fc / sr);      // (pi * fc / sr)
        float t = std::tan(w);                  // tan(pi*fc/sr)
        float _a = (1.0f - t) / (1.0f + t);
        a = clampf(_a, -0.9999f, 0.9999f);
    }

    inline float process(float x){
        float y = -a * x + x1 + a * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

class DFXVintagePhaserStereo {
public:
    enum Stages { STG_2=2, STG_4=4, STG_6=6, STG_8=8, STG_12=12 };

    DFXVintagePhaserStereo(int stages = STG_6)
    {
        setStages(stages);
        setRate(0.2f);
        setDepth(1.0f);
        setCenterHz(700.0f);
        setRangeOctaves(3.0f);
        setFeedback(0.55f);
        setMix(0.5f);
        setWidth(0.8f);
        setDrive(0.25f);

        mDrift.setRateHz(0.12f, (float)gam::sampleRate());
    }

    // --- Parameters ---
    void setStages(int st){
        st = (int)clampf((float)st, 2.f, 12.f);
        if(st % 2) st += 1;                // even stages preferred for classic phasers
        mStages = st;
        for(int ch=0; ch<2; ++ch){
            mAP[ch].resize(mStages);
            reset();
        }
    }

    void setRate(float hz)          { mRate = std::max(0.f, hz); }
    void setDepth(float d01)        { mDepth = clampf(d01, 0.f, 1.f); }      // 0..1
    void setCenterHz(float hz)      { mCenterHz = std::max(20.f, hz); }
    void setRangeOctaves(float oct) { mRangeOct = clampf(oct, 0.25f, 6.0f); } // sweep range
    void setFeedback(float fb)      { mFeedback = clampf(fb, -0.95f, 0.95f); }
    void setMix(float mix)          { mMix = clampf(mix, 0.f, 1.f); }
    void setWidth(float w)          { mWidth = clampf(w, 0.f, 1.f); }
    void setDrive(float d)          { mDrive = std::max(0.f, d); }

    // Optional: “classic vibe” switch: adds a bit more drift & less perfect periodicity
    void setDriftAmount(float a01)  { mDriftAmt = clampf(a01, 0.f, 1.f); }

    void reset(){
        for(int ch=0; ch<2; ++ch){
            for(auto& ap : mAP[ch]) ap.reset();
            mFbState[ch] = 0.0f;
        }
    }

    inline void process(float inL, float inR, float& outL, float& outR)
    {
        float sr = (float)gam::sampleRate();

        // Base LFO (sine + triangle blend feels “optical / analog”)
        float s = std::sinf(mPhase);
        float tri = (2.f / float(M_PI)) * std::asinf(clampf(s, -1.f, 1.f));
        float lfo = 0.6f * s + 0.4f * tri;

        // Drift (slow wandering) — subtle
        float drift = mDrift.next() * (0.20f * mDriftAmt);

        // Stereo decorrelation: quadrature + slight rate mismatch
        float sR   = std::sinf(mPhase + float(M_PI_2));
        float triR = (2.f / float(M_PI)) * std::asinf(clampf(sR, -1.f, 1.f));
        float lfoR = 0.6f * sR + 0.4f * triR;

        // advance phase
        float rateL = mRate;
        float rateR = mRate * (1.0f + 0.01f * mWidth);
        mPhase += 2.f * float(M_PI) * (rateL / sr);
        if(mPhase > 2.f * float(M_PI)) mPhase -= 2.f * float(M_PI);

        // Convert LFO into a swept center frequency (log sweep in octaves)
        // fc = center * 2^(range * depth * lfo)
        float sweepExpL = (mRangeOct * mDepth) * (lfo + drift);   // [-range..+range]
        float sweepExpR = (mRangeOct * mDepth) * (lfoR + drift);

        float fcL = mCenterHz * std::exp2(sweepExpL);
        float fcR = mCenterHz * std::exp2(sweepExpR);

        // Safety clamp
        fcL = clampf(fcL, 30.0f, 0.45f * sr);
        fcR = clampf(fcR, 30.0f, 0.45f * sr);

        // Update AP coefficients (cheap; 1st-order)
        for(int i=0;i<mStages;i++){
            mAP[0][i].setFreq(fcL, sr);
            mAP[1][i].setFreq(fcR, sr);
        }

        // Process each channel
        float yL = channelProcess(0, inL, fcL);
        float yR = channelProcess(1, inR, fcR);

        // Width control in mid/side (post effect)
        float mid  = 0.5f * (yL + yR);
        float side = 0.5f * (yL - yR) * mWidth;
        yL = mid + side;
        yR = mid - side;

        // Equal-power dry/wet
        float theta = mMix * float(M_PI_2);
        float dryG  = std::cos(theta);
        float wetG  = std::sin(theta);

        outL = inL * dryG + yL * wetG;
        outR = inR * dryG + yR * wetG;
    }

private:
    inline float channelProcess(int ch, float x, float /*fc*/)
    {
        // Feedback/resonance (regen) with gentle saturation
        float fb = softsat(mFbState[ch], mDrive);
        float u  = x + mFeedback * fb;

        float y = u;
        for(int i=0;i<mStages;i++)
            y = mAP[ch][i].process(y);

        // Store for next sample feedback
        mFbState[ch] = y;

        // Classic phaser output is typically "mix input with allpass output"
        // You can also expose “notch depth” separately; here wet path already contains it.
        // A subtle extra blend helps vintage character:
        return 0.5f * (x + y);
    }

private:
    int mStages = 6;
    std::vector<Allpass1> mAP[2];

    float mRate     = 0.2f;
    float mDepth    = 1.0f;
    float mCenterHz = 700.0f;
    float mRangeOct = 3.0f;
    float mFeedback = 0.55f;
    float mMix      = 0.5f;
    float mWidth    = 0.8f;
    float mDrive    = 0.25f;

    float mPhase = 0.f;

    float mFbState[2] = {0.f, 0.f};

    DriftNoise mDrift;
    float mDriftAmt = 0.35f;
};
