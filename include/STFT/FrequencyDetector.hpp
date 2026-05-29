#pragma once
#include <algorithm>
#include <cmath>


class FrequencyDetector {
public:
    FrequencyDetector()
    : mSmoothHz(0)
    {}

    // Optional tuning
    void setRange(float minHz, float maxHz) {
        mMinHz = std::max((float)1, minHz);
        mMaxHz = std::max(mMinHz + (float)1, maxHz);
    }

    // Hysteresis thresholds in normalized signal units
    void setHysteresis(float lo, float hi) {
        mLo = std::min(lo, hi);
        mHi = std::max(lo, hi);
    }

    // Simple DC blocker: y[n] = x[n] - x[n-1] + R*y[n-1]
    void setDcBlockR(float r) { mDcR = std::clamp(r, (float)0, (float)0.999999f); }

    float process(float x)
    {
        const float fs = (float)gam::sampleRate();

        // DC block
        float y = x - mPrevX + mDcR * mPrevY;
        mPrevX = x;
        mPrevY = y;

        // Count samples since last accepted crossing
        mCounter++;

        // Schmitt trigger state machine
        // We measure period on rising "armed -> high" transitions.
        if (!mHigh) {
            if (y >= mHi) {
                // Rising edge accepted
                onEdge(fs);
                mHigh = true;
            }
        } else {
            if (y <= mLo) {
                // Re-arm once we go low enough
                mHigh = false;
            }
        }

        // Timeout: if too long since last edge, confidence decays and hold last freq
        const int maxPeriod = (int)(fs / std::max((float)1, mMinHz));
        if (mCounter > maxPeriod * 2) {
            mConfidence *= (float)0.995;          // slow decay
            mConfidence = std::max(mConfidence, (float)0);
        }

        // Confidence-weighted smoothing: more stable -> faster lock
        float alpha = lerp((float)0.9995, (float)0.90, mConfidence); // slow -> fast
        mSmoothHz = alpha * mSmoothHz + ((float)1 - alpha) * mTargetHz;

        return mSmoothHz;
    }

    float confidence() const { return mConfidence; }
    float targetHz() const { return mTargetHz; }

private:
    void onEdge(float fs)
    {
        // Ignore first edge (no period yet)
        if (mLastPeriod > 0) {
            int p = mCounter;

            // Sanity: clamp to allowed freq range
            float hz = fs / (float)p;
            if (hz >= mMinHz && hz <= mMaxHz) {
                // Period stability confidence: compare against previous period (and smoothed period)
                float dp = std::abs((float)p - (float)mLastPeriod);
                float rel = dp / std::max((float)1, (float)mLastPeriod);

                // Map relative jitter to confidence: 0 jitter -> 1, large jitter -> 0
                float c = (float)1 - std::clamp(rel * (float)4, (float)0, (float)1);

                // Smooth confidence a bit
                mConfidence = (float)0.9 * mConfidence + (float)0.1 * c;

                mTargetHz = hz;
                mLastPeriod = p;
            } else {
                // Out of range -> reduce confidence slightly
                mConfidence *= (float)0.98;
            }
        } else {
            mLastPeriod = mCounter;
        }

        mCounter = 0;
    }

    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // DC blocker state
    float mPrevX = 0;
    float mPrevY = 0;
    float mDcR   = (float)0.995;

    // Schmitt trigger state
    bool   mHigh = false;
    float mLo   = (float)-0.02;
    float mHi   = (float) 0.02;

    // Period tracking
    int    mCounter    = 0;
    int    mLastPeriod = 0;

    // Output
    float mMinHz      = (float)20;
    float mMaxHz      = (float)5000;

    float mTargetHz   = 0;
    float mSmoothHz   = 0;
    float mConfidence = (float)0;
};
