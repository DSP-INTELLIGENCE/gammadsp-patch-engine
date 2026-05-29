// WavetableEngine.hpp
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <complex>

#include "Gamma/Access.h"
#include "Gamma/Containers.h"

// -----------------------------
// Helpers
// -----------------------------
namespace wt {

inline float lerp(float a, float b, float t){ return a + (b-a)*t; }

inline uint32_t floatToPhase32(float phase01){
    // phase01 in [0,1). Wrap safely.
    phase01 -= std::floor(phase01);
    constexpr double TWO32 = 4294967296.0; // 2^32
    return (uint32_t)(phase01 * (float)TWO32);
}

// common clamp
template<class T>
inline T clamp(T v, T lo, T hi){ return std::max(lo, std::min(hi, v)); }

// pick common serum-ish frame sizes
inline int inferFrameSize(uint64_t totalSamples){
    const int candidates[] = {4096, 2048, 1024, 512, 256};
    for(int s: candidates){
        if(totalSamples % (uint64_t)s == 0) return s;
    }
    // fallback: largest power-of-two divisor <= 4096
    int best = 0;
    for(int s=4096; s>=64; s>>=1){
        if(totalSamples % (uint64_t)s == 0){ best = s; break; }
    }
    return best ? best : 2048;
}

} // namespace wt


// -----------------------------
// WTFrame / Wavetable
// -----------------------------
struct WTFrame {
    // mip[0] = highest bandwidth (largest size). mip[k] size halves each level.
    std::vector<gam::ArrayPow2<float>> mip;
};

struct Wavetable {
    uint32_t baseSize   = 0;     // e.g. 2048
    uint32_t frameCount = 0;     // e.g. 256
    std::vector<WTFrame> frames;

    bool valid() const {
        return baseSize > 0 && frameCount > 0 && frames.size() == frameCount && !frames.empty()
               && !frames[0].mip.empty() && frames[0].mip[0].size() == baseSize;
    }

    uint32_t mipLevels() const {
        return frames.empty() ? 0u : (uint32_t)frames[0].mip.size();
    }
};

// -----------------------------
// Mip selection (frequency → mip level)
// -----------------------------
struct MipSelector {
    // Crossfade between mip levels using log2(phaseInc * baseSize).
    // - phaseInc = freq / sampleRate
    // Returns: level0, level1, frac in [0,1]
    static inline void select(float phaseInc, uint32_t baseSize, uint32_t maxLevel,
                              uint32_t& l0, uint32_t& l1, float& frac)
    {
        // heuristic: higher phaseInc => smaller mip
        // x ~ cycles-per-sample * tableLength
        float x = std::max(1e-12f, phaseInc * (float)baseSize);

        float lf = std::log2(x);                 // continuous
        float cl = wt::clamp(lf, 0.0f, (float)maxLevel);

        l0 = (uint32_t)std::floor(cl);
        l1 = std::min(l0 + 1, maxLevel);
        frac = cl - (float)l0;
    }
};


// -----------------------------
// Table lookup (fixed-point phase + linear interpolation)
// -----------------------------
inline float lookupPow2Linear(const gam::ArrayPow2<float>& t, uint32_t ph){
    const uint32_t i  = t.index(ph);
    const float frac  = t.fraction(ph);
    const uint32_t mask = t.size() - 1; // pow2

    const float a = t[i];
    const float b = t[(i + 1) & mask];
    return wt::lerp(a, b, frac);
}


// -----------------------------
// WavetableOsc (step 1–4 runtime)
// -----------------------------
struct WavetableOsc {
    const Wavetable* wt = nullptr;

    float sampleRate = 48000.f;
    float freqHz     = 440.f;
    float morph01    = 0.f;    // 0..1 across frames

    uint32_t ph = 0;           // 32-bit phase accumulator

    void setWavetable(const Wavetable* w){ wt = w; }
    void setSampleRate(float sr){ sampleRate = sr; }
    void setFreq(float f){ freqHz = std::max(0.f, f); }
    void setMorph(float m){ morph01 = wt::clamp(m, 0.f, 1.f); }
    void resetPhase(float phase01 = 0.f){ ph = wt::floatToPhase32(phase01); }

    // Render into a buffer (block-based, no allocations).
    // Uses:
    // - frame morphing (linear between adjacent frames)
    // - mip selection (and optional mip crossfade)
    void render(gam::Slice<float> out){
        if(!wt || !wt->valid()){
            out.set(0.f);
            return;
        }

        const uint32_t framesN = wt->frameCount;
        const uint32_t maxMip  = wt->mipLevels() - 1;
        const uint32_t baseSz  = wt->baseSize;

        // Frame selection
        float pos = morph01 * (float)(framesN - 1);
        uint32_t f0 = (uint32_t)std::floor(pos);
        uint32_t f1 = std::min(f0 + 1, framesN - 1);
        float fFrac = pos - (float)f0;

        // Phase increment
        float phaseInc = freqHz / std::max(1.f, sampleRate);
        uint32_t inc = (uint32_t)(phaseInc * 4294967296.0f); // 2^32

        // Mip selection (block-constant; for audio-rate freq mod you’d need per-sample inc/level)
        uint32_t m0, m1;
        float mFrac;
        MipSelector::select(phaseInc, baseSz, maxMip, m0, m1, mFrac);

        const auto& A0 = wt->frames[f0].mip[m0];
        const auto& A1 = wt->frames[f1].mip[m0];

        const auto& B0 = wt->frames[f0].mip[m1];
        const auto& B1 = wt->frames[f1].mip[m1];

        const int N = out.count();
        for(int i=0;i<N;++i){
            // sample at mip m0
            float sA0 = lookupPow2Linear(A0, ph);
            float sA1 = lookupPow2Linear(A1, ph);
            float sA  = wt::lerp(sA0, sA1, fFrac); // frame morph

            // sample at mip m1 (for crossfade)
            float sB0 = lookupPow2Linear(B0, ph);
            float sB1 = lookupPow2Linear(B1, ph);
            float sB  = wt::lerp(sB0, sB1, fFrac);

            // mip crossfade
            out[i] = wt::lerp(sA, sB, mFrac);

            ph += inc;
        }
    }
};


// ============================================================================
// Serum .wav loader skeleton (dr_wav)
// ============================================================================
//
// Serum wavetables are typically stored as a mono WAV containing:
//   frameCount * frameSize samples, frames concatenated.
//
// You requested a skeleton: this is minimal glue around dr_wav.
// Bring your own dr_wav files:
//   - dr_wav.h (https://github.com/mackron/dr_libs)
//   - Define DR_WAV_IMPLEMENTATION in exactly one .cpp
//
// ============================================================================

// Forward declare dr_wav API types (include dr_wav.h in your .cpp)
struct drwav;

namespace serum_wav {

struct LoadedWav {
    std::vector<float> samples;  // interleaved if channels > 1; we’ll downmix
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
};

// Implement in your .cpp after including dr_wav.h
bool loadWavF32(const char* path, LoadedWav& out);

// Convert loaded wav to mono (if needed), return mono samples
inline std::vector<float> toMono(const LoadedWav& w){
    if(w.channels == 1) return w.samples;
    std::vector<float> mono;
    mono.resize(w.samples.size() / w.channels);
    for(size_t i=0, j=0; j<mono.size(); ++j){
        double acc = 0.0;
        for(uint32_t ch=0; ch<w.channels; ++ch) acc += w.samples[i++];
        mono[j] = (float)(acc / (double)w.channels);
    }
    return mono;
}

} // namespace serum_wav


// ============================================================================
// Mip builders (Decimate and FFT "brickwall")
// ============================================================================

namespace mipbuild {

// -----------------------------
// Common per-frame conditioning
// -----------------------------
inline void removeDC(std::vector<float>& frame){
    double sum = 0.0;
    for(float v: frame) sum += v;
    float mean = (float)(sum / (double)frame.size());
    for(float& v: frame) v -= mean;
}

inline void normalizePeak(std::vector<float>& frame, float target=1.f){
    float peak = 0.f;
    for(float v: frame) peak = std::max(peak, std::abs(v));
    if(peak > 1e-12f){
        float g = target / peak;
        for(float& v: frame) v *= g;
    }
}

inline void ensurePow2FrameSize(std::vector<float>& frame, int sizePow2){
    // Assumes frame.size() == sizePow2 already in normal use.
    // If not, you can resample; skeleton keeps it strict.
    (void)frame; (void)sizePow2;
}

// -----------------------------
// Build mip chain by decimation (fast, good)
// Each level: lowpass-ish (box) then downsample by 2.
// -----------------------------
inline WTFrame buildFrameMips_Decimate(const float* frameData, int frameSize){
    std::vector<float> current(frameData, frameData + frameSize);

    removeDC(current);
    normalizePeak(current);

    WTFrame fr;

    int sz = frameSize;
    while(sz >= 16){
        gam::ArrayPow2<float> level((uint32_t)sz);
        for(int i=0;i<sz;++i) level[i] = current[i];
        fr.mip.push_back(level);

        if(sz == 16) break;

        // downsample
        std::vector<float> next(sz/2);
        for(int i=0;i<sz/2;++i){
            // simple 2-tap averager (box LPF)
            next[i] = 0.5f * (current[2*i] + current[2*i + 1]);
        }
        current.swap(next);
        sz >>= 1;
    }

    return fr;
}


// -----------------------------
// Minimal FFT (power-of-two, complex) for offline wavetable building
// Cooley–Tukey iterative radix-2 (not the fastest, but fine for load-time).
// -----------------------------
inline void fft_inplace(std::vector<std::complex<float>>& a, bool inverse){
    const size_t n = a.size();
    // bit-reversal
    for(size_t i=1, j=0; i<n; ++i){
        size_t bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) std::swap(a[i], a[j]);
    }

    for(size_t len=2; len<=n; len<<=1){
        float ang = (inverse ? +1.f : -1.f) * 2.f * (float)M_PI / (float)len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for(size_t i=0; i<n; i+=len){
            std::complex<float> w(1.f, 0.f);
            for(size_t j=0; j<len/2; ++j){
                auto u = a[i + j];
                auto v = a[i + j + len/2] * w;
                a[i + j] = u + v;
                a[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }

    if(inverse){
        float invN = 1.f / (float)n;
        for(auto& x: a) x *= invN;
    }
}

// Build brickwall-limited mip levels via FFT:
// - FFT the base frame once
// - For each mip, zero bins above its nyquist and iFFT
inline WTFrame buildFrameMips_FFTBrickwall(const float* frameData, int frameSize){
    std::vector<float> base(frameData, frameData + frameSize);
    removeDC(base);
    normalizePeak(base);

    // complex buffer
    std::vector<std::complex<float>> spec(frameSize);
    for(int i=0;i<frameSize;++i) spec[i] = std::complex<float>(base[i], 0.f);

    // FFT base
    fft_inplace(spec, /*inverse=*/false);

    WTFrame fr;
    int sz = frameSize;
    int level = 0;

    while(sz >= 16){
        // For mip level L, effective sample rate is halved L times.
        // Brickwall cutoff: keep only bins up to (N/2) / 2^L.
        // This is a practical alias-control rule for wavetable playback.
        int n = frameSize;
        int maxBin = (n/2) >> level; // inclusive-ish
        maxBin = std::max(1, std::min(maxBin, n/2));

        // copy spectrum and zero highs
        std::vector<std::complex<float>> s = spec;

        // zero bins (maxBin+1 .. n-maxBin-1)
        for(int k=maxBin+1; k < n - maxBin; ++k) s[k] = 0.f;

        // iFFT to time domain (still length n)
        fft_inplace(s, /*inverse=*/true);

        // downsample to sz by picking every 2^level sample
        gam::ArrayPow2<float> table((uint32_t)sz);
        int step = 1 << level;
        for(int i=0;i<sz;++i){
            table[i] = s[i * step].real();
        }

        // optional normalize per-level (keeps perceived loudness steadier)
        {
            float peak = 0.f;
            for(uint32_t i=0;i<table.size();++i) peak = std::max(peak, std::abs(table[i]));
            if(peak > 1e-12f){
                float g = 1.f / peak;
                for(uint32_t i=0;i<table.size();++i) table[i] *= g;
            }
        }

        fr.mip.push_back(table);

        if(sz == 16) break;
        sz >>= 1;
        level++;
    }

    return fr;
}

} // namespace mipbuild


// ============================================================================
// Build a Wavetable from a Serum WAV (frames concatenated) using either mip builder
// ============================================================================
enum class MipBuildMode { Decimate, FFTBrickwall };

inline bool loadSerumWavetable(const char* wavPath,
                               Wavetable& outWT,
                               int frameSizeHint = 0,
                               MipBuildMode mode = MipBuildMode::Decimate)
{
    serum_wav::LoadedWav w;
    if(!serum_wav::loadWavF32(wavPath, w)) return false;

    std::vector<float> mono = serum_wav::toMono(w);
    if(mono.empty()) return false;

    int frameSize = frameSizeHint > 0 ? frameSizeHint : wt::inferFrameSize((uint64_t)mono.size());
    if(frameSize <= 0) return false;
    if((mono.size() % (size_t)frameSize) != 0) return false;

    int frameCount = (int)(mono.size() / (size_t)frameSize);

    outWT.baseSize = (uint32_t)frameSize;
    outWT.frameCount = (uint32_t)frameCount;
    outWT.frames.clear();
    outWT.frames.reserve(frameCount);

    for(int f=0; f<frameCount; ++f){
        const float* framePtr = mono.data() + (size_t)f * (size_t)frameSize;

        WTFrame fr =
            (mode == MipBuildMode::FFTBrickwall)
            ? mipbuild::buildFrameMips_FFTBrickwall(framePtr, frameSize)
            : mipbuild::buildFrameMips_Decimate(framePtr, frameSize);

        outWT.frames.push_back(std::move(fr));
    }

    return outWT.valid();
}
