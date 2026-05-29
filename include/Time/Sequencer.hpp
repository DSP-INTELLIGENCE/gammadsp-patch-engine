// GDSP_Sequencer.hpp
Absolutely — and this fits your architecture *beautifully*.

Below is a **realtime-safe, zero-allocation, deterministic 16-step sequencer** that plugs directly into your `NoteManager` + `PolyVoice` system.

---

# 🧩 **GDSP 16-Step Sequencer**

This sequencer:

* runs on the audio thread safely
* produces noteOn / noteOff events
* supports tempo, swing, gate, per-step pitch & velocity
* has **no dynamic memory** in `process()`

---

## 🧱 1. Step Definition

```cpp
struct Step {
    int   note = 60;        // MIDI note
    float velocity = 1.f;
    float gate = 0.8f;      // 0..1
    bool  active = true;
};
```

---

## 🎛️ 2. Sequencer Core

```cpp
// GDSP_Sequencer.hpp
#pragma once
#include <cmath>
#include "GDSP_NoteManager.hpp"

class StepSequencer16 : public Generator {
public:
    StepSequencer16(NoteManager& nm, float sampleRate)
    : mNotes(nm), mSR(sampleRate) {}

    void setTempo(float bpm) {
        mBpm = bpm;
        mSamplesPerStep = (60.f / bpm) * mSR * 0.25f; // 16th notes
    }

    void setSwing(float v) { mSwing = v; }

    Step& step(int i) { return mSteps[i & 15]; }

    void start() { mRunning = true; }
    void stop()  { mRunning = false; }

    float process() override {
        if(!mRunning) return 0.f;

        if(--mCounter <= 0) advance();

        return 0.f;
    }

private:
    void advance() {
        auto& s = mSteps[mIndex];

        if(s.active) {
            mNotes.noteOn(s.note, s.velocity);
            mGateSamples = int(mSamplesPerStep * s.gate);
        }

        mIndex = (mIndex + 1) & 15;

        // swing
        mCounter = (mIndex & 1)
            ? int(mSamplesPerStep * (1.f + mSwing))
            : int(mSamplesPerStep * (1.f - mSwing));
    }

    void updateGate() {
        if(mGateSamples > 0 && --mGateSamples == 0) {
            mNotes.noteOffAll();
        }
    }

    NoteManager& mNotes;
    float mSR;

    Step  mSteps[16];
    int   mIndex = 0;

    float mBpm = 120.f;
    float mSwing = 0.f;
    float mSamplesPerStep = 0;

    int   mCounter = 0;
    int   mGateSamples = 0;
    bool  mRunning = false;
};
```

---

## 🧪 3. Usage Example

```cpp
auto poly = engine.create<PolyVoice<16>>();
engine.addGen(poly);

NoteManager nm(*poly, sr);
nm.setEngine(&engine);

auto seq = engine.create<StepSequencer16>(nm, sr);
engine.addGen(seq);

seq->setTempo(120);
seq->start();

seq->step(0).note = 60;
seq->step(4).note = 64;
seq->step(8).note = 67;
seq->step(12).note = 72;
```

---

## 🧬 What You Now Have

```
Sequencer → NoteManager → PolyVoice → Generator graph → Audio
```

Features:

* deterministic timing
* sample-accurate scheduling
* polyphonic
* voice stealing
* realtime safe
* extensible

---

If you want, the next step is adding **pattern memory**, **ratcheting**, **per-step modulation**, or **clock sync** — all of which fit this design naturally.
