// OTA Phaser
#include <vector>
#include <cmath>
#include <algorithm>

static inline float fast_tan(float x) { return std::tanf(x); }
static inline float exp2f_fast(float x) { return std::exp2f(x); }

// 1st-order allpass stage (OTA phaser stage analog)
struct Allpass1 {
    float a  = 0.0f;   // coefficient
    float x1 = 0.0f;   // x[n-1]
    float y1 = 0.0f;   // y[n-1]

    void reset() { x1 = y1 = 0.0f; }

    inline float process(float x) {
        float y = -a * x + x1 + a * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

// simple 1-pole smoothing for parameters (per-sample)
struct OnePoleSmooth {
    float y = 0.0f;
    float g = 0.0f; // 0..1, closer to 1 = faster

    void reset(float v) { y = v; }
    void setTimeMs(float ms, float sr) {
        // classic 1-pole: g = 1 - exp(-1/(tau*sr))
        float tau = std::max(0.000001f, ms * 0.001f);
        g = 1.0f - std::exp(-1.0f / (tau * sr));
    }
    inline float process(float x) {
        y += g * (x - y);
        return y;
    }
};

class OTAPhaser {
public:
    explicit OTAPhaser(int stages = 4, float sampleRate = 48000.0f)
    : mSR(sampleRate)
    {
        setStages(stages);
        // sane defaults
        setRate(0.4f);
        setBaseHz(800.0f);
        setDepthOct(2.0f);      // sweep ±2 octaves (typical phaser range)
        setFeedback(0.6f);      // regen
        setMix(0.5f);

        mCoefSmooth.setTimeMs(3.0f, mSR); // smooth coefficient a (reduces zipper)
        mCoefSmooth.reset(0.0f);
    }

    void setSampleRate(float sr) {
        mSR = std::max(8000.0f, sr);
        mCoefSmooth.setTimeMs(mCoefSmoothMs, mSR);
    }

    void setStages(int stages) {
        mStages = std::clamp(stages, 1, 12);
        mAP.assign(mStages, Allpass1{});
        reset();
    }

    void reset() {
        for (auto& s : mAP) s.reset();
        mPhase = 0.0f;
        mFbState = 0.0f;
    }

    // Controls
    void setRate(float hz)          { mRateHz = std::max(0.0f, hz); }
    void setBaseHz(float hz)        { mBaseHz = std::clamp(hz, 20.0f, 8000.0f); }
    void setDepthOct(float oct)     { mDepthOct = std::clamp(oct, 0.0f, 6.0f); }   // octaves
    void setFeedback(float fb)      { mFeedback = std::clamp(fb, -0.99f, 0.99f); } // regen
    void setMix(float mix)          { mMix = std::clamp(mix, 0.0f, 1.0f); }
    void setOutputGain(float g)     { mOutGain = std::max(0.0f, g); }

    // Optional: “color” like Small Stone: pushes notches deeper via feedback saturation
    void setDrive(float d)          { mDrive = std::max(0.0f, d); }

    // Optional: coefficient smoothing time
    void setCoefSmoothMs(float ms)  {
        mCoefSmoothMs = std::max(0.0f, ms);
        mCoefSmooth.setTimeMs(mCoefSmoothMs, mSR);
    }

    float process(float x) {
        // LFO (sine)
        float lfo = std::sinf(mPhase);
        mPhase += 2.0f * float(M_PI) * mRateHz / mSR;
        if (mPhase > 2.0f * float(M_PI)) mPhase -= 2.0f * float(M_PI);

        // Analog-like exponential sweep in octaves
        float fc = mBaseHz * exp2f_fast(lfo * mDepthOct);
        fc = std::clamp(fc, 20.0f, 0.45f * mSR); // keep stable margin

        // Convert fc -> allpass coefficient a
        // a = (1 - tan(pi*fc/sr)) / (1 + tan(pi*fc/sr))
        float t = fast_tan(float(M_PI) * (fc / mSR));
        float aTarget = (1.0f - t) / (1.0f + t);

        // Smooth the coefficient to avoid zippering when fc moves quickly
        float a = mCoefSmooth.process(aTarget);

        // Feedback (regen) path — classic OTA phaser “swirl”
        float xin = x + mFeedback * mFbState;

        // Optional soft clip in regen path for analog-ish behavior
        if (mDrive > 0.0f) {
            float k = 1.0f + mDrive * 4.0f;
            xin = std::tanh(k * xin) / std::tanh(k);
        }

        // Cascade allpass stages
        float y = xin;
        for (auto& s : mAP) {
            s.a = a;         // same OTA control to each stage (classic)
            y = s.process(y);
        }

        mFbState = y;

        // Equal-power dry/wet mix
        float theta = mMix * float(M_PI_2);
        float out = x * std::cos(theta) + y * std::sin(theta);

        return out * mOutGain;
    }

private:
    float mSR = 48000.0f;

    int   mStages = 4;
    std::vector<Allpass1> mAP;

    float mPhase   = 0.0f;
    float mRateHz  = 0.4f;

    float mBaseHz  = 800.0f;
    float mDepthOct= 2.0f;

    float mFeedback= 0.6f;
    float mMix     = 0.5f;
    float mOutGain = 1.0f;

    float mDrive   = 0.0f;

    float mFbState = 0.0f;

    OnePoleSmooth mCoefSmooth;
    float mCoefSmoothMs = 3.0f;
};
