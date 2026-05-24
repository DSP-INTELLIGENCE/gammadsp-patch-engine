#pragma once
#include <cmath>
#include <algorithm>
#include "TEngine.hpp"
#include "TParameters.hpp"
#include "TModDelay.hpp"
#include "TOnePole.hpp"
#include "TBlockDC.hpp"
#include "TLFO.hpp"

// -------------------------------------------------
// Moog System 55 Echo Machine (Template)
// -------------------------------------------------
template<typename T>
class TMoogEchoMachine {
public:
    explicit TMoogEchoMachine(T maxDelaySec = T(2))
    : mDelayL(maxDelaySec, T(0.25))
    , mDelayR(maxDelaySec, T(0.251))
    , mToneL(T(6000))
    , mToneR(T(6000))
    , mWowL(T(0.2)), mWowR(T(0.2))
    , mFlutterL(T(6)), mFlutterR(T(6))
    {
        mDelayL.setIpolType(gam::ipl::CUBIC);
        mDelayR.setIpolType(gam::ipl::CUBIC);

        // phase offsets = stereo movement
        mWowR.phaseAdd(T(M_PI_2));
        mFlutterR.phaseAdd(T(M_PI_2));

        setTime(T(0.35));
        setFeedback(T(0.55));
        setMix(T(0.45));
        setTone(T(6000));
        setDrive(T(0.4));
        setCross(T(0.15));

        setWow(T(0.25), T(0.003));
        setFlutter(T(6.0), T(0.0006));
    }

    // ---------- PATCHABLE CONTROLS ----------

    inline void setTime(T sec) noexcept {
        mBaseDelay = std::clamp(sec, T(0.001), mDelayL.getMaxDelayTime());
    }

    inline void setFeedback(T fb) noexcept {
        mFeedback = std::clamp(fb, T(0), T(0.95));
    }

    inline void setMix(T m) noexcept {
        mMix = std::clamp(m, T(0), T(1));
    }

    inline void setTone(T cutoffHz) noexcept {
        cutoffHz = std::clamp(
            cutoffHz, T(50),
            T(0.45) * T(gam::sampleRate())
        );
        mToneL.setFreq(cutoffHz);
        mToneR.setFreq(cutoffHz);
    }

    inline void setDrive(T d) noexcept {
        mDrive = std::max(T(0), d);
    }

    inline void setCross(T c) noexcept {
        mCross = std::clamp(c, T(0), T(1));
    }

    inline void setWow(T rateHz, T depthSec) noexcept {
        mWowDepth = std::max(T(0), depthSec);
        mWowL.setFreq(std::max(T(0.01), rateHz));
        mWowR.setFreq(std::max(T(0.01), rateHz));
    }

    inline void setFlutter(T rateHz, T depthSec) noexcept {
        mFlutterDepth = std::max(T(0), depthSec);
        mFlutterL.setFreq(std::max(T(0.1), rateHz));
        mFlutterR.setFreq(std::max(T(0.1), rateHz));
    }

    inline void reset() noexcept {
        mDelayL.clear();
        mDelayR.clear();
        mDC_L.reset();
        mDC_R.reset();
    }

    // ---------- DSP ----------

    inline void process(T in, T& outL, T& outR) noexcept
    {
        // --- wow/flutter modulation (seconds) ---
        const T modL =
            mWowDepth     * mWowL.process() +
            mFlutterDepth * mFlutterL.process();

        const T modR =
            mWowDepth     * mWowR.process() +
            mFlutterDepth * mFlutterR.process();

        // additive modulation (tape-like)
        T dL = mBaseDelay + modL;
        T dR = mBaseDelay + modR;

        const T minDelay = T(0.0005);
        const T maxDelay = mDelayL.getMaxDelayTime();
        dL = std::clamp(dL, minDelay, maxDelay);
        dR = std::clamp(dR, minDelay, maxDelay);

        // tiny slew (~2 ms) to avoid zipper
        const T g = T(1) - std::exp(-T(1) / (T(0.002) * T(gam::sampleRate())));
        mDLs += g * (dL - mDLs);
        mDRs += g * (dR - mDRs);

        mDelayL.setDelay(mDLs);
        mDelayR.setDelay(mDRs);

        // --- read ---
        const T dl = mDelayL.read();
        const T dr = mDelayR.read();

        // --- tape saturation ---
        T satL = softSat(dl);
        T satR = softSat(dr);

        satL = mDC_L.process(satL);
        satR = mDC_R.process(satR);

        // --- HF loss ---
        const T fbL = mToneL.process(satL);
        const T fbR = mToneR.process(satR);

        // --- feedback safety ---
        const T modNorm = (mWowDepth + mFlutterDepth)
                          / std::max(T(0.001), mBaseDelay);

        T fbSafe = mFeedback * (T(1) - T(0.25) * modNorm);
        fbSafe = std::clamp(fbSafe, T(0), T(0.95));

        // --- write ---
        mDelayL.write(in + (fbL + mCross * fbR) * fbSafe);
        mDelayR.write(in + (fbR + mCross * fbL) * fbSafe);

        // --- equal-power mix ---
        const T theta = mMix * T(M_PI_2);
        const T dryG  = std::cos(theta);
        const T wetG  = std::sin(theta);

        outL = in * dryG + dl * wetG;
        outR = in * dryG + dr * wetG;
    }

private:
    inline T softSat(T x) const noexcept {
        if (mDrive <= T(0)) return x;
        const T k = T(1) + mDrive * T(8);
        return std::tanh(x * k) / std::tanh(k);
    }

private:
    // delay
    TModDelay<T> mDelayL;
    TModDelay<T> mDelayR;

    // feedback tone
    TOnePoleLP<T> mToneL;
    TOnePoleLP<T> mToneR;

    // DC blocking
    TBlockDC<T> mDC_L, mDC_R;

    // modulation
    TLFO<T> mWowL, mWowR;
    TLFO<T> mFlutterL, mFlutterR;

    // parameters
    T mBaseDelay    { T(0.35) };
    T mFeedback     { T(0.55) };
    T mMix          { T(0.45) };
    T mDrive        { T(0.4) };
    T mCross        { T(0.15) };

    T mWowDepth     { T(0.003) };
    T mFlutterDepth { T(0.0006) };

    // smoothed delay
    T mDLs { T(0.35) };
    T mDRs { T(0.351) };
};
