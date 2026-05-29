#pragma once
#include <cmath>
#include <algorithm>

// InputGain: transparent excitation control.
// - Smooths in dB domain via your ParamLinear
// - Converts to linear gain per-sample
// - One shared gain for stereo safety
class InputGain : public Function
{
public:
    InputGain(float initDb = 0.0f, float smoothMs = 10.0f)
    {
        // Attach your smoother to the parameter
        mSmooth.setTime(smoothMs);
        mGainDb.attachSmoother(&mSmooth);
        reset(initDb);
    }

    // --- Controls ---
    void setGainDb(float db)               { mGainDb.set(db); }
    float gainDb() const                   { return mGainDb._value; } // raw param value
    void setSmoothingMs(float ms)          { mSmooth.setTime(std::max(0.0f, ms)); }
    bool isRamping() const                 { return mSmooth.isRamping(); }

    void reset(float db = 0.0f)
    {
        // Hard set + reset ramp state
        mGainDb._value = db;
        mSmooth = ParamLinear(db, mSmoothMs());  // re-init ramp at current db
        mGainDb.attachSmoother(&mSmooth);
        mLastLin = dbToLin(db);
    }

    // --- DSP (mono) ---
    float process(float x) override
    {
        // Smooth dB -> linear
        float db  = mGainDb.process();
        float lin = dbToLin(db);

        // Denormal guard for very small gains
        lin += 1e-24f;

        mLastLin = lin;
        return x * lin;
    }

    // --- Stereo helper (recommended) ---
    inline StereoSample processStereo(float inL, float inR)
    {
        float db  = mGainDb.process();
        float lin = dbToLin(db);
        lin += 1e-24f;

        mLastLin = lin;
        return StereoSample(inL * lin, inR * lin);
    }

    // --- Buffer helpers ---
    void run(const float* in, float* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

    void runStereo(const float* inL, const float* inR,
                   float* outL, float* outR, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            StereoSample s = processStereo(inL[i], inR[i]);
            outL[i] = s.outL;
            outR[i] = s.outR;
        }
    }

    // For metering / downstream use
    float gainLinear() const { return mLastLin; }

private:
    static inline float dbToLin(float db)
    {
        // 10^(dB/20)
        return std::pow(10.0f, db * (1.0f / 20.0f));
    }

    float mSmoothMs() const { return mSmooth.value(); } // not critical; just keeps reset tidy

private:
    Parameter   mGainDb;
    ParamLinear mSmooth { 0.0f, 10.0f };
    float       mLastLin = 1.0f;
};
