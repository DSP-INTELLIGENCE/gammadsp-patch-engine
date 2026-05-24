#pragma once
#include "TPT1Pole.hpp"
#include "Biquad.hpp"

struct MarshallToneStack : public ToneStack
{
    // --- Filters ---
    Biquad bass;     // low shelf
    Biquad mid;      // peaking
    Biquad treble;   // high shelf

    // --- Internal gain normalization ---
    float makeup = 1.0f;

    void prepare(float sr) override
    {
        // Bass shelf ~100 Hz
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(100.f);
        bass.setRes(0.707f);
        bass.setLevel(0.f);  // dB, controlled dynamically

        // Mid band ~650 Hz (Marshall signature)
        mid.setType(gam::PEAKING);
        mid.setFreq(650.f);
        mid.setRes(0.7f);
        mid.setLevel(0.f);

        // Treble shelf ~4 kHz
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(4000.f);
        treble.setRes(0.707f);
        treble.setLevel(0.f);

        // Output trim (prevents level jump)
        makeup = 0.85f;
    }

    void reset() override
    {
        bass.reset();
        mid.reset();
        treble.reset();
    }

    // bass01, mid01, treble01 ∈ [0,1]
    float process(float x, float bass01, float mid01, float treble01)
    {
        // ----- Control law (Marshall-like) -----
        // Bass: ±10 dB
        float bassDB = -10.f + 20.f * bass01;

        // Mid: deep scoop to boost (+8 dB to -12 dB)
        float midDB  = -12.f + 20.f * mid01;

        // Treble: ±12 dB
        float trebDB = -12.f + 24.f * treble01;

        bass.setLevel(bassDB);
        mid.setLevel(midDB);
        treble.setLevel(trebDB);

        // ----- Filter order matters -----
        float y = x;
        y = bass.process(y);     // low end first
        y = mid.process(y);      // mid shaping
        y = treble.process(y);   // sparkle last

        return y * makeup;
    }
};
