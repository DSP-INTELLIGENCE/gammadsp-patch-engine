// Read Pointer System + Fractional Interpolator Upgrade (Linear / Cubic / Windowed-Sinc)
//
// What you get:
// - DelayLine with explicit ReadPointer objects (support multi-tap cleanly)
// - Fractional delay reads using:
//    * Linear
//    * Cubic (Catmull-Rom)
//    * Windowed-sinc (Blackman window, configurable taps)
//
// Realtime-safe: no allocation in process. Sinc kernel precomputed in prepare().
//
// Usage idea:
//   DelayLine dl; dl.prepare(sr, maxDelaySec, 64 /*sinc taps*/);
//   dl.write(x);
//   ReadPointer rp; rp.setDelaySamples(1234.56f);
//   float y = dl.read(rp, Interp::Sinc);

#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

class FracDelayLine
{
public:
    enum class Interp { Linear, Cubic, Sinc };

    // A "read head" (tap). Keep many of these for multi-tap.
    struct ReadPointer
    {
        // delay in samples (can be fractional)
        float delaySamples = 0.0f;

        // optional: if you want to slew the delay per-read head, do it outside
        inline void setDelaySamples(float d) { delaySamples = d; }
    };

    DelayLine() = default;

    // sincTaps must be even and >= 8 for decent quality; 32-128 typical.
    void prepare(double sampleRate, double maxDelaySeconds, int sincTaps = 64)
    {
        sr_ = (sampleRate > 1.0) ? sampleRate : 48000.0;

        // Buffer size + guard for interpolation neighborhood
        const int maxSamples = static_cast<int>(std::ceil(maxDelaySeconds * sr_)) + kGuardSamples;
        buffer_.assign(std::max(1, maxSamples), 0.0f);
        size_ = static_cast<int>(buffer_.size());
        writeIdx_ = 0;

        // Sinc setup (precompute a phase table)
        setSincTaps(sincTaps);
        buildSincTable();
        clear();
    }

    void clear()
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        writeIdx_ = 0;
    }

    inline void write(float x)
    {
        buffer_[writeIdx_] = x;
        ++writeIdx_;
        if (writeIdx_ >= size_) writeIdx_ = 0;
    }

    // Main read entry point (per read pointer)
    inline float read(const ReadPointer& rp, Interp interp) const
    {
        // Read position is writeIdx - delaySamples (writeIdx points to "next write" slot,
        // which is fine as long as you use the same convention consistently).
        float readPos = static_cast<float>(writeIdx_) - rp.delaySamples;

        // wrap into [0, size)
        readPos = wrapFloat(readPos);

        switch (interp)
        {
            case Interp::Linear: return readLinear(readPos);
            case Interp::Cubic:  return readCubicCatmullRom(readPos);
            case Interp::Sinc:   return readWindowedSinc(readPos);
            default:             return readCubicCatmullRom(readPos);
        }
    }

    int size() const { return size_; }

private:
    // ---- Linear ----
    inline float readLinear(float readPos) const
    {
        const int i0 = static_cast<int>(readPos);
        const int i1 = (i0 + 1) % size_;
        const float frac = readPos - static_cast<float>(i0);

        const float a = buffer_[i0];
        const float b = buffer_[i1];
        return a + frac * (b - a);
    }

    // ---- Cubic (Catmull-Rom) ----
    inline float readCubicCatmullRom(float readPos) const
    {
        const int i1 = static_cast<int>(readPos);
        const float t = readPos - static_cast<float>(i1);

        const int i0 = wrapInt(i1 - 1);
        const int i2 = wrapInt(i1 + 1);
        const int i3 = wrapInt(i1 + 2);

        const float y0 = buffer_[i0];
        const float y1 = buffer_[i1];
        const float y2 = buffer_[i2];
        const float y3 = buffer_[i3];

        // Catmull-Rom spline coefficients
        const float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
        const float a1 =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
        const float a2 = -0.5f*y0 + 0.5f*y2;
        const float a3 =              y1;

        return ((a0*t + a1)*t + a2)*t + a3;
    }

    // ---- Windowed-Sinc ----
    //
    // We use a precomputed table of kernels for fractional phases:
    //  - phases: kNumPhases (e.g. 1024)
    //  - taps: sincTaps_ (even)
    //
    // For a readPos = i + frac, we pick phase = round(frac * (kNumPhases-1))
    // and convolve around center i with that kernel.
    inline float readWindowedSinc(float readPos) const
    {
        const int center = static_cast<int>(readPos);
        const float frac = readPos - static_cast<float>(center);

        // phase index [0..kNumPhases-1]
        const int phase = static_cast<int>(std::lround(frac * (kNumPhases - 1)));
        const float* kernel = &sincTable_[phase * sincTaps_];

        // Convolution indices:
        // taps are symmetric around center. For even taps, we use half on each side.
        const int half = sincTaps_ / 2;
        float acc = 0.0f;

        // kernel[k] corresponds to sample at (center + (k - (half - 1))) or similar,
        // but our kernel construction below matches this exact mapping:
        // sampleIndex = center + (k - (half - 1))
        // This makes the "center" align properly for even tap counts.
        for (int k = 0; k < sincTaps_; ++k)
        {
            const int offset = k - (half - 1);
            const int idx = wrapInt(center + offset);
            acc += buffer_[idx] * kernel[k];
        }
        return acc;
    }

private:
    // ---- Sinc table creation ----
    //
    // Sinc kernel: sinc(x) = sin(pi x) / (pi x)
    // Window: Blackman (good sidelobe suppression)
    //
    // We build kernels for frac in [0, 1) at kNumPhases steps.
    // For each frac, taps are centered between samples in a way suitable for even tap counts.
    void setSincTaps(int taps)
    {
        // force even and clamp
        taps = std::max(8, taps);
        if (taps % 2 != 0) taps += 1;
        sincTaps_ = taps;
    }

    static inline float sinc(float x)
    {
        // normalized sinc: sin(pi x)/(pi x)
        if (std::fabs(x) < 1e-8f) return 1.0f;
        const float px = static_cast<float>(M_PI) * x;
        return std::sin(px) / px;
    }

    static inline float blackman(int n, int N)
    {
        // n in [0, N-1]
        // w(n)=0.42 - 0.5 cos(2πn/(N-1)) + 0.08 cos(4πn/(N-1))
        if (N <= 1) return 1.0f;
        const float a0 = 0.42f;
        const float a1 = 0.5f;
        const float a2 = 0.08f;
        const float x  = 2.0f * static_cast<float>(M_PI) * (float)n / (float)(N - 1);
        return a0 - a1 * std::cos(x) + a2 * std::cos(2.0f * x);
    }

    void buildSincTable()
    {
        // allocate phase table
        sincTable_.assign(kNumPhases * sincTaps_, 0.0f);

        const int half = sincTaps_ / 2;

        for (int p = 0; p < kNumPhases; ++p)
        {
            const float frac = (float)p / (float)(kNumPhases - 1); // [0,1]

            float sum = 0.0f;
            float* kernel = &sincTable_[p * sincTaps_];

            // Construct taps.
            // For even taps, we center the kernel between two samples by using offsets:
            // offset = k - (half - 1)  (so offsets are: -(half-1) ... +half)
            // Then we evaluate sinc at (offset - frac).
            // This matches the readWindowedSinc mapping.
            for (int k = 0; k < sincTaps_; ++k)
            {
                const int offset = k - (half - 1);
                const float x = (float)offset - frac;

                const float w = blackman(k, sincTaps_);
                const float v = sinc(x) * w;

                kernel[k] = v;
                sum += v;
            }

            // Normalize kernel for unity gain at DC
            if (std::fabs(sum) > 1e-12f)
            {
                const float inv = 1.0f / sum;
                for (int k = 0; k < sincTaps_; ++k)
                    kernel[k] *= inv;
            }
        }
    }

private:
    // ---- Wrapping helpers ----
    inline int wrapInt(int idx) const
    {
        // Only small +/- offsets are expected. This is fast enough.
        if (idx < 0) idx += size_;
        if (idx >= size_) idx -= size_;
        // if offsets can be larger, replace with true modulo:
        // idx %= size_; if (idx<0) idx += size_;
        return idx;
    }

    inline float wrapFloat(float x) const
    {
        const float s = static_cast<float>(size_);
        while (x < 0.0f) x += s;
        while (x >= s)  x -= s;
        return x;
    }

private:
    static constexpr int kGuardSamples = 4;
    static constexpr int kNumPhases = 1024; // phase resolution for sinc table

    std::vector<float> buffer_;
    int size_ = 0;
    int writeIdx_ = 0;
    double sr_ = 48000.0;

    // Sinc interpolation
    int sincTaps_ = 64;
    std::vector<float> sincTable_;
};

/*
-------------------------
Minimal example:

DelayLine dl;
dl.prepare(48000.0, 2.0, 64);  // 2s max, 64-tap sinc

DelayLine::ReadPointer tap;
tap.setDelaySamples(1234.56f);

for each sample:
  dl.write(x);
  float y_lin  = dl.read(tap, DelayLine::Interp::Linear);
  float y_cub  = dl.read(tap, DelayLine::Interp::Cubic);
  float y_sinc = dl.read(tap, DelayLine::Interp::Sinc);

-------------------------

Notes:
- Sinc gives the best HF preservation for fractional reads, great for pitch/modulated delay,
  but costs O(taps) per read. Use it for high-quality modes or offline.
- Cubic is a great default for realtime: smooth and cheap.
- For multi-tap, keep N ReadPointer objects and call dl.read() for each.
*/
