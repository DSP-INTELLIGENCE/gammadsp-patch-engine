// TimeControlSystem.hpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Forward decls (your engine types)
class Delay;
class MultitapDelay;
class Comb;
class ModClock;
class ControlClock;
class ControlEnvelope;
class ControlStepSequencer;

// Optional: a lightweight parameter smoother (control-rate)
struct Slew {
    float y = 0.0f;
    float a = 1.0f; // smoothing coefficient in [0..1], 1 = no smoothing

    void setTime(float seconds, float sampleRate) {
        seconds = std::max(0.0f, seconds);
        if (seconds <= 0.0f) { a = 1.0f; return; }
        // One-pole: a = 1 - exp(-1/(tau*sr))  (tau ~= seconds)
        a = 1.0f - std::exp(-1.0f / (seconds * sampleRate));
        a = std::clamp(a, 0.0f, 1.0f);
    }

    float process(float x) {
        y += a * (x - y);
        return y;
    }

    void reset(float v) { y = v; }
};

enum class NoteDiv : uint8_t {
    N1, N1_2, N1_4, N1_8, N1_16, N1_32,
    DOTTED_1_4, DOTTED_1_8, DOTTED_1_16,
    TRIPLET_1_4, TRIPLET_1_8, TRIPLET_1_16
};

enum class TransportState : uint8_t { Stop, Play };

enum class SyncSource : uint8_t {
    FreeRun,     // internal clock only
    Host,        // host BPM/transport (if you wire it)
    ExternalMTC  // placeholder for future SMPTE/MTC
};

enum class TimeUnit : uint8_t {
    Seconds,
    Samples,
    Beats
};

struct TransportInfo {
    TransportState state = TransportState::Stop;
    double bpm = 120.0;
    double sampleRate = 48000.0;

    // phase in [0,1) of the beat (or bar), depends on your choice
    double beatPhase = 0.0;

    // Optional musical context
    int32_t timeSigNum = 4;
    int32_t timeSigDen = 4;

    // sample timeline (useful for scheduling)
    int64_t sampleTime = 0;
};

struct QuantizeSpec {
    NoteDiv grid = NoteDiv::N1_16;
    float   strength = 1.0f;     // 0 = no quantize, 1 = hard snap
    bool    enabled = true;
};

struct CrossfadeSpec {
    // Used for glitch-free pointer jumps (delay time changes, loop jumps, reverse, etc.)
    float seconds = 0.005f;      // 5ms default
    bool  enabled = true;
};

struct LookaheadSpec {
    float seconds = 0.0f;        // 0 means no lookahead
    bool  enabled = false;
};

// A sample-accurate event scheduler (interface only)
using EventId = uint32_t;

enum class EventType : uint8_t {
    NoteOn,
    NoteOff,
    Trigger,
    Marker,
    SetParam
};

struct ScheduledEvent {
    EventType type;
    int64_t   sampleAt;   // absolute sample time on the system timeline
    uint32_t  a = 0;       // payload (voice, id, param index, etc.)
    float     v = 0.0f;    // payload value
};

struct DueEvent {
    ScheduledEvent e;
    uint32_t offset;   // sample offset inside current block
};

/// The orchestration layer over your primitives + Gamma Domain.
/// Owns tempo/transport model, scheduling, quantization, smoothing policies,
/// and produces stable timing signals for FX/synth modules.
class TimeControlSystem {
public:
    // ---------- lifecycle ----------
    TimeControlSystem() = default;

    // Call when Gamma Domain / engine SR changes
    void setSampleRate(double sr) {
        sr = std::max(1.0, sr);
        mTransport.sampleRate = sr;
        mDelayTimeSlew.setTime(mDelayTimeSlewSec, (float)sr);
        mBpmSlew.setTime(mBpmSlewSec, (float)sr);
        updateDerived();
    }

    void reset() {
        mTransport.sampleTime = 0;
        mTransport.beatPhase = 0.0;
        mLastBeatPhase = 0.0;
        mLastBarPhase  = 0.0;
        mTransport.state = TransportState::Stop;

        mPendingEvents.clear();
        mDueEvents.clear();
        mNextEventId = 1;

        // Reset smoothers to current targets
        mBpmSlew.reset((float)mTransport.bpm);
        mDelayTimeSlew.reset(mDelayTimeTargetSec);

        updateDerived();
    }

    // ---------- sync / transport ----------
    void setSyncSource(SyncSource src) { mSyncSource = src; }

    void setTransportState(TransportState s) { mTransport.state = s; }

    void setTimeSignature(int num, int den) {
        mTransport.timeSigNum = std::max(1, num);
        mTransport.timeSigDen = std::max(1, den);
        updateDerived();
    }

    // internal tempo target (even in Host mode you may want smoothing)
    void setBPM(double bpm) {
        bpm = std::clamp(bpm, 1.0, 999.0);
        mBpmTarget = (float)bpm;
    }

    double bpm() const { return mTransport.bpm; }
    double sampleRate() const { return mTransport.sampleRate; }
    int64_t sampleTime() const { return mTransport.sampleTime; }

    // If you have host timing, call this each block before processBlock().
    // (Interface-only: wire to your host sync layer later)
    void setHostTransport(double hostBpm, TransportState hostState, double hostBeatPhase01) {
        mHostBpm = (float)std::clamp(hostBpm, 1.0, 999.0);
        mHostState = hostState;
        mHostBeatPhase01 = hostBeatPhase01 - std::floor(hostBeatPhase01);
        mHasHostTransport = true;
    }

    // ---------- musical time conversions ----------
    double secondsPerBeat() const { return 60.0 / mTransport.bpm; }

    double noteDivToBeats(NoteDiv d) const {
        // beats relative to quarter note = 1 beat if 4/4 (common definition).
        // Here "beat" means quarter-note beat.
        switch (d) {
            case NoteDiv::N1:      return 4.0;
            case NoteDiv::N1_2:    return 2.0;
            case NoteDiv::N1_4:    return 1.0;
            case NoteDiv::N1_8:    return 0.5;
            case NoteDiv::N1_16:   return 0.25;
            case NoteDiv::N1_32:   return 0.125;

            case NoteDiv::DOTTED_1_4:  return 1.5;
            case NoteDiv::DOTTED_1_8:  return 0.75;
            case NoteDiv::DOTTED_1_16: return 0.375;

            case NoteDiv::TRIPLET_1_4:  return 2.0/3.0;
            case NoteDiv::TRIPLET_1_8:  return 1.0/3.0;
            case NoteDiv::TRIPLET_1_16: return 1.0/6.0;
        }
        return 1.0;
    }

    double noteDivToSeconds(NoteDiv d) const {
        return secondsPerBeat() * noteDivToBeats(d);
    }

    int64_t secondsToSamples(double sec) const {
        return (int64_t)std::llround(sec * mTransport.sampleRate);
    }

    // ---------- quantization & scheduling ----------
    void setQuantizeSpec(const QuantizeSpec& q) { mQuant = q; }
    void setCrossfadeSpec(const CrossfadeSpec& x) { mXfade = x; }
    void setLookaheadSpec(const LookaheadSpec& l) { mLookahead = l; }

    // Schedule an event in musical time (beats from now), quantized optionally.
    EventId scheduleInBeats(double beatsFromNow,
                            EventType type,
                            uint32_t a = 0,
                            float v = 0.0f)
    {
        const double currentBeat = mTransport.beatPhase;
        const double nowBeatAbs  = currentBeat + (double)(mTransport.sampleTime) * (mTransport.bpm / 60.0) / mTransport.sampleRate;

        double targetBeat = nowBeatAbs + beatsFromNow;

        // Quantization
        if (mQuant.enabled) {
            const double grid = noteDivToBeats(mQuant.grid);
            const double snapped = std::round(targetBeat / grid) * grid;
            targetBeat = targetBeat + (snapped - targetBeat) * mQuant.strength;
        }

        const double deltaBeats   = targetBeat - nowBeatAbs;
        const double seconds     = deltaBeats * secondsPerBeat();
        const int64_t targetSample = mTransport.sampleTime + secondsToSamples(seconds);

        return scheduleAtSample(targetSample, type, a, v);
    }

    // Schedule at absolute sample time (system timeline)
    EventId scheduleAtSample(int64_t sampleAt, EventType type, uint32_t a = 0, float v = 0.0f) {
        ScheduledEvent e{type, sampleAt, a, v};
        mPendingEvents.push_back(e);
        return mNextEventId++;
    }

    // Pull due events for this audio block after processBlock() updates time
    const std::vector<ScheduledEvent>& dueEvents() const { return mDueEvents; }

    // ---------- clock outputs (queries) ----------
    // These are derived signals modules can read each sample or per block.
    double beatPhase01() const { return mTransport.beatPhase; }

    // One-sample pulses (updated in processBlock)
    bool beatTick() const { return mBeatTick; }
    bool barTick()  const { return mBarTick;  }

    // Subdiv tick derived from beat phase; div=2 => eighths, div=4 => sixteenths, etc.
    bool tickDiv(int div) const { return (div > 0) ? mDivTick[clampDivIndex(div)] : false; }

    // ---------- time targets for modules (semantic controls) ----------
    // Example: central place to set a "global delay time" target that modules can subscribe to.
    void setGlobalDelayTimeSeconds(float sec, float slewSeconds = 0.02f) {
        sec = std::max(0.0f, sec);
        mDelayTimeTargetSec = sec;
        mDelayTimeSlewSec = std::max(0.0f, slewSeconds);
        mDelayTimeSlew.setTime(mDelayTimeSlewSec, (float)mTransport.sampleRate);
    }

    void setGlobalDelayTimeNoteDiv(NoteDiv div, float slewSeconds = 0.02f) {
        setGlobalDelayTimeSeconds((float)noteDivToSeconds(div), slewSeconds);
    }

    float globalDelayTimeSecondsSmoothed() const { return mDelayTimeSmoothedSec; }


    // ---------- block processing ----------
    // Call once per audio block. Advances internal time, updates tempo/phase,
    // computes ticks, resolves scheduling, and updates smoothed control signals.
    void processBlock(size_t numSamples)
    {
        const int64_t blockStart = mTransport.sampleTime;
        const int64_t blockEnd   = blockStart + (int64_t)numSamples;

        // 1) Choose transport source
        advanceTransportSource();

        // 2) Smooth BPM
        const float bpmTarget = (mSyncSource == SyncSource::Host && mHasHostTransport)
                            ? mHostBpm
                            : mBpmTarget;

        const float bpmSmoothed = mBpmSlew.process(bpmTarget);
        mTransport.bpm = std::clamp((double)bpmSmoothed, 1.0, 999.0);

        // 3) Advance phase
        const double beatsPerSec    = mTransport.bpm / 60.0;
        const double beatsPerSample = beatsPerSec / mTransport.sampleRate;
        const double beatAdvance    = beatsPerSample * (double)numSamples;

        mLastBeatPhase = mTransport.beatPhase;
        mTransport.beatPhase = wrap01(mTransport.beatPhase + beatAdvance);

        const double barLenBeats = (double)mTransport.timeSigNum * (4.0 / (double)mTransport.timeSigDen);

        mLastBarPhase = mBarPhase;
        mBarPhase = wrap01(mBarPhase + (beatAdvance / barLenBeats));

        // 4) Block-level ticks
        mBeatTick = (mTransport.beatPhase < mLastBeatPhase);
        mBarTick  = (mBarPhase < mLastBarPhase);

        computeDivTicks();

        // 5) Update smoothed semantic controls
        mDelayTimeSmoothedSec = mDelayTimeSlew.process(mDelayTimeTargetSec);

        // 6) Resolve due events *inside this block*
        mDueEvents.clear();

        for (auto& e : mPendingEvents) {
            if (e.sampleAt >= blockStart && e.sampleAt < blockEnd) {
                DueEvent d;
                d.e = e;
                d.offset = (uint32_t)(e.sampleAt - blockStart);
                mDueEvents.push_back(d);
            }
        }

        // 7) Remove consumed events (no erase in loop, RT-safe)
        auto keep = std::remove_if(
            mPendingEvents.begin(), mPendingEvents.end(),
            [&](const ScheduledEvent& e){ return e.sampleAt < blockEnd; }
        );
        mPendingEvents.erase(keep, mPendingEvents.end());

        // 8) Advance timeline
        mTransport.sampleTime = blockEnd;
    }


private:
    // ---------- internal helpers ----------
    static double wrap01(double x) {
        x -= std::floor(x);
        return x;
    }

    static int clampDivIndex(int div) {
        // maps common divisions to an index [0..kMaxDivSlots-1]
        // For arbitrary divs, you can add a map. Here we clamp to a fixed set.
        static constexpr int kDivs[] = {2, 3, 4, 6, 8, 12, 16, 24, 32};
        for (int i = 0; i < (int)(sizeof(kDivs)/sizeof(kDivs[0])); ++i)
            if (kDivs[i] == div) return i;
        return 0; // fallback to /2
    }

    void computeDivTicks() {
        static constexpr int kDivs[] = {2, 3, 4, 6, 8, 12, 16, 24, 32};
        for (int i = 0; i < kMaxDivSlots; ++i) {
            const int div = kDivs[i];
            const int curr = (int)(mTransport.beatPhase * div);
            const int prev = (int)(mLastBeatPhase * div);
            mDivTick[i] = (curr != prev);
        }
    }

    void resolveDueEvents() {
        mDueEvents.clear();
        const int64_t now = mTransport.sampleTime;

        // Pull due events (stable, no allocations if you reserve)
        // Simple O(N): fine for small event counts. Use a min-heap later if needed.
        auto it = mPendingEvents.begin();
        while (it != mPendingEvents.end()) {
            if (it->sampleAt <= now) {
                mDueEvents.push_back(*it);
                it = mPendingEvents.erase(it);
            } else {
                ++it;
            }
        }
    }

    void advanceTransportSource() {
        if (mSyncSource == SyncSource::Host && mHasHostTransport) {
            mTransport.state = mHostState;
            // Optional: hard-lock beat phase to host to avoid drift
            // If you prefer PLL behavior, replace this with a phase correction filter.
            mTransport.beatPhase = wrap01(mHostBeatPhase01);
        } else {
            // internal state already in mTransport
        }
    }

    void updateDerived() {
        // reserve to avoid RT allocations during normal operation
        mPendingEvents.reserve(256);
        mDueEvents.reserve(256);
    }

private:
    // ---------- configuration ----------
    SyncSource    mSyncSource = SyncSource::FreeRun;
    QuantizeSpec  mQuant{};
    CrossfadeSpec mXfade{};
    LookaheadSpec mLookahead{};

    // ---------- transport ----------
    TransportInfo mTransport{};
    double mBarPhase = 0.0;
    double mLastBeatPhase = 0.0;
    double mLastBarPhase  = 0.0;

    bool mBeatTick = false;
    bool mBarTick  = false;

    static constexpr int kMaxDivSlots = 9;
    std::array<bool, kMaxDivSlots> mDivTick{};

    // Host inputs (optional)
    bool mHasHostTransport = false;
    float mHostBpm = 120.0f;
    TransportState mHostState = TransportState::Stop;
    double mHostBeatPhase01 = 0.0;

    // ---------- smoothing / targets ----------
    float mBpmTarget = 120.0f;
    Slew  mBpmSlew{};
    float mBpmSlewSec = 0.05f;

    float mDelayTimeTargetSec = 0.25f;
    float mDelayTimeSmoothedSec = 0.25f;
    Slew  mDelayTimeSlew{};
    float mDelayTimeSlewSec = 0.02f;

    // ---------- scheduler ----------
    std::vector<ScheduledEvent> mPendingEvents;
    std::vector<ScheduledEvent> mDueEvents;
    EventId mNextEventId = 1;
};
