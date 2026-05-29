// Core Time Engine for a delay plugin (single channel)
// - Circular buffer delay line
// - Write pointer / read pointer
// - Fractional delay with interpolation (Linear, Cubic)
// - Parameter smoothing (anti-zip)
// - Optional tempo sync + quantized note divisions
//
// Drop this into your project and call processSample() per-sample.
//
// NOTE: This is the "time engine" only (no feedback/tone/etc).

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

class TimeEngine
{
public:
  enum class Interp { Linear, Cubic };

  TimeEngine() = default;

  // Must be called before processing.
  // maxDelaySeconds should be the maximum delay time your plugin supports.
  void prepare(double sampleRate, double maxDelaySeconds, Interp interp = Interp::Cubic)
  {
    sr_ = (sampleRate > 1.0) ? sampleRate : 48000.0;
    interp_ = interp;

    const int maxSamples = static_cast<int>(std::ceil(maxDelaySeconds * sr_)) + kGuardSamples;
    buffer_.assign(std::max(1, maxSamples), 0.0f);
    size_ = static_cast<int>(buffer_.size());
    writeIdx_ = 0;

    // Default to 250ms
    setDelaySeconds(0.25);
    reset();
  }

  void reset()
  {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIdx_ = 0;
    smoothDelaySamples_.reset(static_cast<float>(targetDelaySamples_));
  }

  // --- Delay time controls (free time) ---
  void setDelaySeconds(double seconds)
  {
    seconds = std::max(0.0, seconds);
    setDelaySamples(seconds * sr_);
  }

  void setDelaySamples(double samples)
  {
    // Clamp to valid safe range (leave room for guard samples for cubic)
    const double maxSafe = static_cast<double>(size_ - kGuardSamples - 1);
    targetDelaySamples_ = std::clamp(samples, 0.0, maxSafe);
  }

  // --- Tempo sync controls ---
  // Call setTempoBPM whenever host BPM changes. Provide transport/clock elsewhere if needed.
  void setTempoBPM(double bpm)
  {
    bpm_ = std::clamp(bpm, 1.0, 999.0);
  }

  // Quantized musical divisions (common set; add more if you want)
  enum class NoteDiv {
    Whole,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    ThirtySecond,
    QuarterTriplet,
    EighthTriplet,
    SixteenthTriplet,
    QuarterDotted,
    EighthDotted,
    SixteenthDotted
  };

  // If tempo sync enabled, delay time is derived from bpm + division.
  // If disabled, setDelaySeconds()/setDelaySamples() takes over.
  void setTempoSyncEnabled(bool enabled) { tempoSyncEnabled_ = enabled; }
  void setNoteDivision(NoteDiv div)      { division_ = div; }

  // You can optionally “quantize” free-time delay (e.g., snap to a grid in ms).
  // Set quantizeStepSeconds <= 0 to disable.
  void setTimeQuantizeStepSeconds(double stepSeconds)
  {
    quantizeStepSeconds_ = stepSeconds;
  }

  // Smoothing time for delay-time changes (ms). Bigger = smoother, less zipper/click.
  void setDelayTimeSmoothingMs(double ms)
  {
    smoothDelaySamples_.setTimeMs(ms, sr_);
  }

  void setInterpolation(Interp i) { interp_ = i; }

  // Process one sample, returns delayed output (wet tap).
  // Feed this into your mixer/feedback network externally.
  float processSample(float x)
  {
    // 1) Update target delay if tempo sync is enabled
    if (tempoSyncEnabled_) {
      const double sec = secondsForDivision(bpm_, division_);
      setDelaySeconds(sec);
    }

    // 2) Optional quantizer (works on whichever target is currently active)
    if (quantizeStepSeconds_ > 0.0) {
      const double stepSamp = quantizeStepSeconds_ * sr_;
      if (stepSamp > 1e-9) {
        targetDelaySamples_ = std::round(targetDelaySamples_ / stepSamp) * stepSamp;
      }
    }

    // 3) Smooth delay time to avoid zipper/clicks
    const float delaySamples = smoothDelaySamples_.process(static_cast<float>(targetDelaySamples_));

    // 4) Write input to buffer
    buffer_[writeIdx_] = x;

    // 5) Read from buffer with fractional delay
    const float y = readDelayed(delaySamples);

    // 6) Advance write pointer
    incrementWrite();

    return y;
  }

  // Convenience: process a block
  void processBlock(const float* in, float* out, int n)
  {
    for (int i = 0; i < n; ++i)
      out[i] = processSample(in[i]);
  }

  // Expose current smoothed delay time (samples)
  float getCurrentDelaySamples() const { return smoothDelaySamples_.value(); }
  double getSampleRate() const { return sr_; }

private:
  // Guard samples for cubic interpolation (needs i-1, i, i+1, i+2)
  static constexpr int kGuardSamples = 4;

  void incrementWrite()
  {
    ++writeIdx_;
    if (writeIdx_ >= size_) writeIdx_ = 0;
  }

  // Read pointer is (writeIdx - delaySamples). We do modulo by wrapping.
  float readDelayed(float delaySamples) const
  {
    // Floating read index in [0, size)
    float readPos = static_cast<float>(writeIdx_) - delaySamples;
    // wrap
    while (readPos < 0.0f) readPos += static_cast<float>(size_);
    while (readPos >= static_cast<float>(size_)) readPos -= static_cast<float>(size_);

    if (interp_ == Interp::Linear) return readLinear(readPos);
    return readCubic(readPos);
  }

  float readLinear(float readPos) const
  {
    const int i0 = static_cast<int>(readPos);
    const int i1 = (i0 + 1) % size_;
    const float frac = readPos - static_cast<float>(i0);

    const float a = buffer_[i0];
    const float b = buffer_[i1];
    return a + frac * (b - a);
  }

  // Catmull-Rom cubic interpolation (musical + common in delays)
  float readCubic(float readPos) const
  {
    const int i1 = static_cast<int>(readPos);
    const float frac = readPos - static_cast<float>(i1);

    const int i0 = wrap(i1 - 1);
    const int i2 = wrap(i1 + 1);
    const int i3 = wrap(i1 + 2);

    const float y0 = buffer_[i0];
    const float y1 = buffer_[i1];
    const float y2 = buffer_[i2];
    const float y3 = buffer_[i3];

    // Catmull-Rom spline
    const float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    const float a1 =        y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    const float a2 = -0.5f*y0 + 0.5f*y2;
    const float a3 =              y1;

    return ((a0*frac + a1)*frac + a2)*frac + a3;
  }

  int wrap(int idx) const
  {
    // Fast-ish wrap for small offsets
    if (idx < 0) idx += size_;
    if (idx >= size_) idx -= size_;
    return idx;
  }

  static double secondsForDivision(double bpm, NoteDiv div)
  {
    const double beat = 60.0 / bpm; // quarter note = 1 beat (common DAW convention)
    switch (div) {
      case NoteDiv::Whole:            return beat * 4.0;
      case NoteDiv::Half:             return beat * 2.0;
      case NoteDiv::Quarter:          return beat * 1.0;
      case NoteDiv::Eighth:           return beat * 0.5;
      case NoteDiv::Sixteenth:        return beat * 0.25;
      case NoteDiv::ThirtySecond:     return beat * 0.125;

      case NoteDiv::QuarterTriplet:   return beat * (2.0/3.0);
      case NoteDiv::EighthTriplet:    return beat * (1.0/3.0);
      case NoteDiv::SixteenthTriplet: return beat * (1.0/6.0);

      case NoteDiv::QuarterDotted:    return beat * 1.5;
      case NoteDiv::EighthDotted:     return beat * 0.75;
      case NoteDiv::SixteenthDotted:  return beat * 0.375;
    }
    return beat;
  }

private:
  double sr_ = 48000.0;
  double bpm_ = 120.0;

  std::vector<float> buffer_;
  int size_ = 0;
  int writeIdx_ = 0;

  Interp interp_ = Interp::Cubic;

  // target delay time in samples (may be tempo-derived)
  double targetDelaySamples_ = 0.0;

  // smoothing
  SmoothValue smoothDelaySamples_;

  // tempo sync
  bool tempoSyncEnabled_ = false;
  NoteDiv division_ = NoteDiv::Quarter;

  // optional quantizer for free time
  double quantizeStepSeconds_ = 0.0;
};

/*
--------------------
Example usage:

CoreTimeEngineDelay delay;
delay.prepare(48000.0, 4.0, CoreTimeEngineDelay::Interp::Cubic);
delay.setDelayTimeSmoothingMs(20.0);

// Free time:
delay.setTempoSyncEnabled(false);
delay.setDelaySeconds(0.35);

// Tempo sync:
delay.setTempoBPM(128.0);
delay.setTempoSyncEnabled(true);
delay.setNoteDivision(CoreTimeEngineDelay::NoteDiv::EighthDotted);

for each sample:
  float wet = delay.processSample(input);
  // dry/wet mix + feedback handled elsewhere
--------------------
*/
