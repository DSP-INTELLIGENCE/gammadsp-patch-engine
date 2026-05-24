// GDSP_SpatialEngine.hpp
#pragma once

#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif

#include <algorithm>
#include <cmath>

// If you use these base types elsewhere, keep includes consistent with your project.
// (Not strictly required for this class.)
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"

/*
    Spatial
    -------
    Mono-in -> Stereo-out spatializer:
      1) Direct field via gam::Dist<2>: ITD (delay), distance attenuation, air-absorption LPF, smoothing
      2) Late reverb via two decorrelated ReverbMS tanks (true-stereo) + optional input crossfeed
      3) Motion interpolation: Ramped source/listener positions + yaw (head rotation)
      4) Doppler: emerges naturally from time-varying propagation delay (Dist ramps delay); no extra pitch shifter needed

    Realtime rules:
      - SAFE during audio: setListenerTarget / setSourceTarget / setHeadYawTarget / setNearFar / setLateDecay / setLateDamping / setLateMix / gains
      - NOT SAFE during audio: setLateFlavor(...) (calls resize -> allocates)
*/

class SpatialEngine {
public:

    SpatialEngine(float maxDistDelaySeconds = 0.25f,
            float nearMeters = 0.2f,
            float farMeters  = 25.0f);

    
    // Speed of sound affects propagation delay in Dist
    void setSpeedOfSound(float metersPerSecond);

    // ---------- Listener / Source (immediate) ----------
    void setListener(float x, float y, float z = 0.f);
    void setSource  (float x, float y, float z = 0.f);

    // ---------- Listener / Source (smoothed motion) ----------
    // Smooth over a number of samples (steps >= 1). This is RT-safe.
    void setListenerTarget(float x, float y, float z, unsigned steps);
    void setSourceTarget  (float x, float y, float z, unsigned steps);

    // Convenience: smooth over time in ms (RT-safe)
    void setListenerTargetMs(float x, float y, float z, float timeMs);
    void setSourceTargetMs  (float x, float y, float z, float timeMs);

    // If you "teleport" listener/source, call teleport to avoid ramping from old state.
    void teleportListener(float x, float y, float z = 0.f);
    void teleportSource  (float x, float y, float z = 0.f);

    // ---------- Head rotation ----------
    // Yaw in radians: 0 means ears on +/-X axis in world space aligned with +X.
    void setHeadYaw(float radians);
    void setHeadYawTarget(float radians, unsigned steps);
    void setHeadYawTargetMs(float radians, float timeMs);

    // Ear spacing in meters (distance between ears)
    void setEarSpacing(float meters);

    // ---------- Distance model ----------
    void setNearFar(float nearMeters, float farMeters);
    void setFarGain(float g);     // gain at far distance
    void setToZero(bool b);       // whether attenuation goes to 0 at far
    void setSmoothingBlockSize(int n); // smoothing "block size" used by Dist ramps

    // ---------- Late reverb ----------
    // NOT realtime-safe (allocates / resizes delay lines).
    void setLateFlavor(gam::ReverbFlavor flavor, unsigned baseOffsetSamples = 0);

    // RT-safe
    void setLateDecay(float seconds);
    void setLateDamping(float v);     // Loop1P damping in (-1,1): + = lowpass, - = highpass
    void setLateMix(float m);         // 0..1
    void setReverbCrossfeed(float v); // 0..0.5 recommended

    // ---------- Gains ----------
    void setDirectGain(float g);
    void setWetGain(float g);
    void setOutputGain(float g);

    // ---------- Processing ----------
    inline void process(float in, float& outL, float& outR) {
        // Advance smoothed scene parameters (RT-safe; no allocations)
        advanceScene_();

        // Update per-ear distances (RT-safe). Doppler emerges from varying delay.
        updateDistances_();

        // 1) Direct field (stereo)
        auto lr = mDist(in);
        float dirL = lr[0];
        float dirR = lr[1];

        // 2) True-stereo late reverb with crossfeed
        const float cf = mReverbCrossfeed;
        const float revInL = dirL + dirR * cf;
        const float revInR = dirR + dirL * cf;

        const float wetL = mLateL(revInL);
        const float wetR = mLateR(revInR);

        // 3) Mix
        float yL = dirL * mDirectGain + wetL * (mLateMix * mWetGain);
        float yR = dirR * mDirectGain + wetR * (mLateMix * mWetGain);

        outL = yL * mOutGain;
        outR = yR * mOutGain;
    }

    // Block API: mono input, planar stereo output
    void run(const float* input, float* outL, float* outR, size_t n);

    // Block API: mono input, interleaved stereo output (LRLR...)
    void runInterleaved(const float* input, float* outputInterleavedLR, size_t frames);

private:
    // ---- Helpers ----
    void updateDistances_();
    void advanceScene_();

    static inline float dist3_(float ax, float ay, float az,
                              float bx, float by, float bz)
    {
        float dx = ax - bx, dy = ay - by, dz = az - bz;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }

private:
    float mSR = 44100.f;

    // Direct field (distance cues) to 2 destinations (ears)
    gam::Dist<2, float> mDist;

    // Late reverb (decorrelated tanks)
    gam::ReverbMS<float> mLateL;
    gam::ReverbMS<float> mLateR;

    // Crossfeed between L/R inputs into reverb tanks
    float mReverbCrossfeed = 0.25f;

    // Scene state (current)
    float mListenerX = 0.f, mListenerY = 0.f, mListenerZ = 0.f;
    float mSourceX   = 1.f, mSourceY   = 0.f, mSourceZ   = 0.f;
    float mEarSpacing = 0.18f;
    float mHeadYaw = 0.f;

    // Smoothed motion (targets)
    gam::Ramped<float> mLstX, mLstY, mLstZ;
    gam::Ramped<float> mSrcX, mSrcY, mSrcZ;
    gam::Ramped<float> mYaw;

    // Late reverb params
    float mLateDecay   = 1.8f;
    float mLateDamping = 0.3f;
    float mLateMix     = 0.25f;

    // Gains
    float mDirectGain  = 1.0f;
    float mWetGain     = 1.0f;
    float mOutGain     = 1.0f;

    // A small optimization: only call Dist::dist() when distances changed enough
    float mPrevDL = -1.f;
    float mPrevDR = -1.f;
    float mDistEps = 1e-6f;
};
