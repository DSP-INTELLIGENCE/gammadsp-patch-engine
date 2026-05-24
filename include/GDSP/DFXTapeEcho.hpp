#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

// TapeEcho
//
// Depends on: ModDelay, Delay, Modulator, Function (from your existing code)
// Notes:
// - This class does NOT call ModDelay::update(); it uses ModDelay::_delay directly so we can
//   shape the feedback path (LPF + saturation + noise) like a tape echo.
// - Uses ModDelay modulators dm/fm/mm/am as *external* modulation lanes:
//     dm: extra delay-time ratio modulation
//     fm: feedback multiplier modulation
//     mm: mix multiplier modulation
//     am: output amp modulation

class DFXTapeEcho : public ModDelay
{
public:
    explicit DFXTapeEcho(float maxDelaySeconds = 2.5f)
    : ModDelay()
    , _rng(0xC0FFEEu)
    , _noiseDist(-1.0f, 1.0f)
    {
        // Re-init underlying delay with requested max (ModDelay::_delay is a Delay member).
        // If your Delay wrapper exposes setMaxDelay, use it:
        _delay.setMaxDelay(maxDelaySeconds, true);

        setTime(0.38f);
        setFeedback(0.65f);
        setMix(0.5f);

        setWowRate(0.35f);
        setFlutterRate(6.5f);

        setWow(0.0030f);      // depth as ratio of base time (0.003 = 0.3%)
        setFlutter(0.0008f);  // depth as ratio of base time

        setAge(0.30f);
        setDrive(0.40f);
        setNoise(0.02f);

        setTimeSmoothingMs(25.0f);

        // ModDelay lanes defaults
        setAM(1.0f);
        setDM(0.0f);
        setFM(0.0f);
        setMM(0.0f);
    }

    // --- Primary controls ---
    void setTime(float seconds)
    {
        _targetTime = std::max(0.0001f, seconds);
    }

    void setFeedback(float fb)
    {
        _baseFeedback = std::clamp(fb, -0.9995f, 0.9995f);
    }

    void setMix(float mix)
    {
        _baseMix = std::clamp(mix, 0.0f, 1.0f);
    }

    // --- Tape behavior controls ---
    // Depths are interpreted as *ratio* (fraction of base time), e.g. 0.005 = 0.5%
    void setWow(float amount)
    {
        _wowDepth = std::max(0.0f, amount);
    }

    void setFlutter(float amount)
    {
        _flutterDepth = std::max(0.0f, amount);
    }

    // LFO "rates" are passed to your Modulator. If your Modulator expects Hz, this is Hz.
    // If it expects radians/sample or something else, adapt here.
    void setWowRate(float r)     { _wowLFO.set(r); }
    void setFlutterRate(float r) { _flutterLFO.set(r); }

    // Age is 0..1 (new -> worn)
    void setAge(float a)
    {
        _age = std::clamp(a, 0.0f, 1.0f);

        // Darker with age
        float cutoff = 14000.0f - _age * 11000.0f; // 14 kHz -> 3 kHz
        cutoff = std::clamp(cutoff, 200.0f, 20000.0f);
        _lpf.setCutoff(cutoff);

        // Slightly more instability with age (subtle; user can still override wow/flutter)
        _ageWowBoost     = 1.0f + 0.8f * _age;
        _ageFlutterBoost = 1.0f + 1.2f * _age;

        // Optional: a little extra feedback “softness” with age
        _compAmount = 0.05f + 0.25f * _age;
    }

    // Drive 0..1 recommended
    void setDrive(float d)
    {
        _drive = std::max(0.0f, d);
    }

    // Noise amount (0..1 typical). Scaled by age internally.
    void setNoise(float n)
    {
        _noise = std::max(0.0f, n);
    }

    // Smoothing time for delay-time moves (helps “tape glide” + avoids zipper)
    void setTimeSmoothingMs(float ms)
    {
        ms = std::max(0.0f, ms);
        float t = ms * 0.001f;
        _timeSlew = (t <= 0.0f) ? 1.0f : (1.0f - std::exp(-1.0f / (gam::sampleRate() * t)));
        _timeSlew = std::clamp(_timeSlew, 0.000001f, 1.0f);
    }
    
    float getTime() const     { return _targetTime; }
    float getMix() const      { return _baseMix; }
    float getFeedback() const { return _baseFeedback; }

    // --- Audio ---
    float process(float x) override
    {
        // Smooth base time
        _timeSmoothingMsCached = _timeSmoothingMsCached; // no-op; cached for setSampleRate()
        _currentTime += (_targetTime - _currentTime) * _timeSlew;

        // Build tape modulation (wow + flutter) as ratio of base time
        // Assumes Modulator::process() gives a waveform sample in [-1, 1].
        float wow     = _wowLFO.process()     * (_wowDepth     * _ageWowBoost);
        float flutter = _flutterLFO.process() * (_flutterDepth * _ageFlutterBoost);

        // External delay modulation lane (dm) is treated as an additional ratio
        float extDM = dm.process();

        float ratio = 1.0f + wow + flutter + extDM;
        ratio = std::max(ratio, 0.0001f);

        float delayT = _currentTime * ratio;
        delayT = std::max(delayT, 0.0001f);

        _delay.setDelay(delayT);

        // Read from tape
        float d = _delay.read();

        // Feedback: base * (1 + external fm lane)
        float fb = _baseFeedback * (1.0f + fm.process());
        fb = std::clamp(fb, -0.9995f, 0.9995f);

        // Mix: base * (1 + external mm lane)
        float mix = _baseMix * (1.0f + mm.process());
        mix = std::clamp(mix, 0.0f, 1.0f);

        // --- Tape feedback path shaping ---
        float fbSig = d * fb;

        // Gentle tape compression/soft limiting before saturation (optional but helps)
        fbSig = softCompress(fbSig, _compAmount);

        // Tape saturation (drive scales input into soft clip)
        fbSig = softClip(fbSig, _drive);

        // HF loss on repeats
        fbSig = _lpf.process(fbSig);

        // Add hiss (scaled by age)
        if (_noise > 0.0f)
        {
            float n = _noiseDist(_rng) * _noise * (0.2f + 0.8f * _age);
            fbSig += n;
        }

        // Write back to tape
        _delay.write(x + fbSig);

        // Output mix + external amp lane
        float y = x * (1.0f - mix) + d * mix;
        return y * am.process();
    }

private:
    // Simple one-pole low-pass
    struct OnePoleLPF
    {
        void setCutoff(float cutoffHz, float sr)
        {
            cutoffHz = std::clamp(cutoffHz, 10.0f, 0.49f * sr);
            // One-pole coefficient
            float x = std::exp(-2.0f * 3.14159265358979323846f * cutoffHz / sr);
            a = x;
            b = 1.0f - x;
        }

        float process(float in)
        {
            z = b * in + a * z;
            return z;
        }

        float a = 0.0f, b = 1.0f, z = 0.0f;
    };

    static inline float softClip(float x, float drive)
    {
        // Drive maps 0..1-ish into gain
        float g = 1.0f + drive * 6.0f;
        x *= g;
        // Smooth-ish saturator
        return x / (1.0f + std::abs(x));
    }

    static inline float softCompress(float x, float amt)
    {
        // amt ~ 0..0.3 recommended
        amt = std::clamp(amt, 0.0f, 1.0f);
        // Simple symmetric “knee” compression
        float k = 1.0f + amt * 8.0f;
        return std::tanh(x * k) / std::tanh(k);
    }

private:
    // Tape LFOs
    Modulator _wowLFO;
    Modulator _flutterLFO;

    // Tape EQ
    OnePoleLPF _lpf;

    // Noise
    std::minstd_rand _rng;
    std::uniform_real_distribution<float> _noiseDist;

    // Parameters
    float _targetTime   = 0.38f;
    float _currentTime  = 0.38f;
    float _timeSlew     = 1.0f;   // smoothing coefficient per sample
    float _timeSmoothingMsCached = 25.0f;

    float _baseFeedback = 0.65f;
    float _baseMix      = 0.5f;

    float _wowDepth     = 0.0030f;
    float _flutterDepth = 0.0008f;

    float _age          = 0.30f;
    float _drive        = 0.40f;
    float _noise        = 0.02f;

    float _ageWowBoost     = 1.0f;
    float _ageFlutterBoost = 1.0f;
    float _compAmount      = 0.10f;
};


// Depends on your existing code:
// - Function
// - Modulator
// - MultitapDelay (wrapper around gam::Multitap<double>)
// - gam::sampleRate()

class MultiHeadDelay : public Function
{
public:
    // You can change kMaxHeads if you want more.
    static constexpr unsigned kMaxHeads = 8;

    explicit MultiHeadDelay(float maxDelaySeconds = 2.5f,
                            unsigned heads = 4,
                            float initTimeSeconds = 0.38f)
    : mDelay(maxDelaySeconds, std::max(1u, std::min(heads, kMaxHeads)), initTimeSeconds)
    , _rng(0xC0FFEEu)
    , _noiseDist(-1.0f, 1.0f)
    {
        setHeads(heads);

        // Defaults (tape-ish)
        setTime(initTimeSeconds);
        setFeedback(0.55f);
        setMix(0.45f);

        setDrive(0.25f);
        setTone(0.65f);
        setNoise(0.01f);
        setAge(0.25f);

        setTimeSmoothingMs(25.0f);

        setWowRate(0.40f);
        setFlutterRate(6.00f);
        setWow(0.0020f);
        setFlutter(0.0006f);

        // Default head ratios (Space Echo-ish spread)
        // You can override per project/preset.
        setHeadRatio(0, 1.00f);
        if (mHeads > 1) setHeadRatio(1, 1.33f);
        if (mHeads > 2) setHeadRatio(2, 2.00f);
        if (mHeads > 3) setHeadRatio(3, 2.66f);
        if (mHeads > 4) setHeadRatio(4, 3.50f);
        if (mHeads > 5) setHeadRatio(5, 4.00f);
        if (mHeads > 6) setHeadRatio(6, 4.50f);
        if (mHeads > 7) setHeadRatio(7, 5.00f);

        // Default per-head gains (slightly decaying)
        for (unsigned i = 0; i < kMaxHeads; ++i)
            _headGain[i] = 1.0f;
        normalizeHeadGains();
        applyToneFromAge();
    }

    // ---- Core controls ----
    void setHeads(unsigned heads)
    {
        heads = std::max(1u, std::min(heads, kMaxHeads));
        mHeads = heads;
        mDelay.setNumTaps(mHeads);

        // Keep ratios/gains valid
        for (unsigned i = 0; i < kMaxHeads; ++i)
        {
            if (_ratio[i] <= 0.0f) _ratio[i] = 1.0f;
            _headGain[i] = std::max(0.0f, _headGain[i]);
        }
        normalizeHeadGains();
    }

    unsigned getHeads() const { return mHeads; }

    void setTime(float seconds)
    {
        _targetTime = std::clamp(seconds, 0.0001f, getMaxDelayTime() * 0.98f);
    }

    float getTime() const { return _targetTime; }

    void setMix(float mix)
    {
        _baseMix = std::clamp(mix, 0.0f, 1.0f);
    }

    float getMix() const { return _baseMix; }

    void setFeedback(float fb)
    {
        _baseFeedback = std::clamp(fb, -0.9995f, 0.9995f);
    }

    float getFeedback() const { return _baseFeedback; }

    // ---- Head layout ----
    // Ratio is a multiplier of base time: headTime = baseTime * ratio
    void setHeadRatio(unsigned head, float ratio)
    {
        if (head >= kMaxHeads) return;
        _ratio[head] = std::max(0.05f, ratio);
    }

    float getHeadRatio(unsigned head) const
    {
        if (head >= kMaxHeads) return 1.0f;
        return _ratio[head];
    }

    void setHeadGain(unsigned head, float g)
    {
        if (head >= kMaxHeads) return;
        _headGain[head] = std::max(0.0f, g);
        normalizeHeadGains();
    }

    float getHeadGain(unsigned head) const
    {
        if (head >= kMaxHeads) return 0.0f;
        return _headGain[head];
    }

    // ---- Tape-ish motion ----
    // Depths interpreted as ratio of base time (0.002 = 0.2%)
    void setWow(float amount)        { _wowDepth     = std::max(0.0f, amount); }
    void setFlutter(float amount)    { _flutterDepth = std::max(0.0f, amount); }
    void setWowRate(float r)         { _wowLFO.set(r); }
    void setFlutterRate(float r)     { _flutterLFO.set(r); }

    // ---- Tone/drive/noise/age ----
    // tone: 0..1 (0 = darker, 1 = brighter)
    void setTone(float t)
    {
        _tone = std::clamp(t, 0.0f, 1.0f);
        applyToneFromAge();
    }

    void setDrive(float d) { _drive = std::max(0.0f, d); }
    void setNoise(float n) { _noise = std::max(0.0f, n); }

    // age: 0..1 (new -> worn). Darkens & increases wow/flutter slightly.
    void setAge(float a)
    {
        _age = std::clamp(a, 0.0f, 1.0f);
        _ageWowBoost     = 1.0f + 0.8f * _age;
        _ageFlutterBoost = 1.0f + 1.2f * _age;
        _compAmount      = 0.05f + 0.25f * _age;
        applyToneFromAge();
    }

    float getAge() const { return _age; }

    // Smoothing for base time moves
    void setTimeSmoothingMs(float ms)
    {
        ms = std::max(0.0f, ms);
        _timeSmoothingMsCached = ms;

        float t = ms * 0.001f;
        _timeSlew = (t <= 0.0f) ? 1.0f : (1.0f - std::exp(-1.0f / (gam::sampleRate() * t)));
        _timeSlew = std::clamp(_timeSlew, 0.000001f, 1.0f);
    }

    // External modulation lanes (optional) — same semantics as TapeEcho:
    // dm: additional delay ratio modulation
    // fm: feedback multiplier modulation
    // mm: mix multiplier modulation
    // am: output amp modulation
    Modulator dm, fm, mm, am;

    float process(float x) override
    {
        // Smooth base time
        _currentTime += (_targetTime - _currentTime) * _timeSlew;

        // Build modulation ratio: 1 + wow + flutter + dm
        float wow     = _wowLFO.process()     * (_wowDepth     * _ageWowBoost);
        float flutter = _flutterLFO.process() * (_flutterDepth * _ageFlutterBoost);
        float extDM   = dm.process();

        float ratio = 1.0f + wow + flutter + extDM;
        ratio = std::max(ratio, 0.0001f);

        // Update tap times
        for (unsigned i = 0; i < mHeads; ++i)
        {
            float t = _currentTime * _ratio[i] * ratio;
            t = std::clamp(t, 0.0001f, getMaxDelayTime() * 0.98f);
            mDelay.setTapTime(i, t);
        }

        // Sum heads (apply per-head gains)
        float sum = 0.0f;
        for (unsigned i = 0; i < mHeads; ++i)
            sum += mDelay.read(i) * _headGain[i];

        // Feedback & mix (with external lanes)
        float fb  = _baseFeedback * (1.0f + fm.process());
        float mix = _baseMix      * (1.0f + mm.process());
        fb  = std::clamp(fb,  -0.9995f, 0.9995f);
        mix = std::clamp(mix,  0.0f,    1.0f);

        // Feedback path shaping (tape-ish)
        float fbSig = sum * fb;

        fbSig = softCompress(fbSig, _compAmount);
        fbSig = softClip(fbSig, _drive);
        fbSig = _lpf.process(fbSig);

        if (_noise > 0.0f)
        {
            float n = _noiseDist(_rng) * _noise * (0.2f + 0.8f * _age);
            fbSig += n;
        }

        // Write once (shared record head)
        mDelay.write(x + fbSig);

        // Output
        float y = x * (1.0f - mix) + sum * mix;
        return y * am.process();
    }

    // Convenience
    float getMaxDelayTime() const { return mDelayMaxSeconds; }

    // If you change max delay, rebuild the multitap
    void setMaxDelay(float seconds, bool keepTime = true)
    {
        seconds = std::max(0.01f, seconds);
        float oldTarget = _targetTime;

        mDelayMaxSeconds = seconds;
        mDelay.setMaxDelay(seconds, true);

        if (keepTime)
            setTime(std::min(oldTarget, seconds * 0.98f));
        else
            setTime(std::min(_targetTime, seconds * 0.98f));
    }

private:
    // Simple one-pole low-pass (same idea as your TapeEcho)
    struct OnePoleLPF
    {
        void setCutoff(float cutoffHz, float sr)
        {
            cutoffHz = std::clamp(cutoffHz, 10.0f, 0.49f * sr);
            float x = std::exp(-2.0f * 3.14159265358979323846f * cutoffHz / sr);
            a = x;
            b = 1.0f - x;
        }

        float process(float in)
        {
            z = b * in + a * z;
            return z;
        }

        float a = 0.0f, b = 1.0f, z = 0.0f;
    };

    static inline float softClip(float x, float drive)
    {
        float g = 1.0f + drive * 6.0f;
        x *= g;
        return x / (1.0f + std::abs(x));
    }

    static inline float softCompress(float x, float amt)
    {
        amt = std::clamp(amt, 0.0f, 1.0f);
        float k = 1.0f + amt * 8.0f;
        return std::tanh(x * k) / std::tanh(k);
    }

    void normalizeHeadGains()
    {
        float sum = 0.0f;
        for (unsigned i = 0; i < mHeads; ++i) sum += _headGain[i];
        if (sum <= 0.0f)
        {
            for (unsigned i = 0; i < mHeads; ++i) _headGain[i] = 1.0f / float(mHeads);
            return;
        }
        float inv = 1.0f / sum;
        for (unsigned i = 0; i < mHeads; ++i) _headGain[i] *= inv;
    }

    void applyToneFromAge()
    {
        // Combine "tone" knob + age into cutoff.
        // tone=1 => brighter; tone=0 => darker
        float sr = gam::sampleRate();

        float brightCut = 16000.0f; // max
        float darkCut   = 2500.0f;  // min (tape-ish)

        // Tone knob chooses between dark & bright
        float cutoff = darkCut + (brightCut - darkCut) * _tone;

        // Age pulls cutoff down
        cutoff *= (1.0f - 0.65f * _age);

        cutoff = std::clamp(cutoff, 200.0f, 0.49f * sr);
        _lpf.setCutoff(cutoff, sr);
    }

private:
    // Underlying delay
    MultitapDelay mDelay;
    unsigned mHeads = 4;

    // Store max seconds locally for convenience (MultitapDelay can also expose this if you prefer)
    float mDelayMaxSeconds = 2.5f;

    // Head layout
    std::array<float, kMaxHeads> _ratio    { 1,1,1,1,1,1,1,1 };
    std::array<float, kMaxHeads> _headGain { 1,1,1,1,1,1,1,1 };

    // Tape modulation
    Modulator _wowLFO;
    Modulator _flutterLFO;

    float _wowDepth     = 0.0020f;
    float _flutterDepth = 0.0006f;

    float _age          = 0.25f;
    float _tone         = 0.65f;
    float _drive        = 0.25f;
    float _noise        = 0.01f;

    float _ageWowBoost     = 1.0f;
    float _ageFlutterBoost = 1.0f;
    float _compAmount      = 0.10f;

    // Time smoothing
    float _targetTime   = 0.38f;
    float _currentTime  = 0.38f;
    float _timeSlew     = 1.0f;
    float _timeSmoothingMsCached = 25.0f;

    // Feedback + mix
    float _baseFeedback = 0.55f;
    float _baseMix      = 0.45f;

    // Tone shaping
    OnePoleLPF _lpf;

    // Noise
    std::minstd_rand _rng;
    std::uniform_real_distribution<float> _noiseDist;
};
