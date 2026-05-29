#pragma once
#include "Biquad.hpp"
#include "ToneStack.hpp"

struct VoxACTopBoostToneStack : public ToneStack
{
    // --- Filters ---
    Biquad bass;        // low shelf
    Biquad treble;      // high shelf
    Biquad upperMid;    // fixed nasal peak

    float makeup = 0.75f;   // less loss than Fender, more than Marshall

    void prepare(float sr) override
    {
        // Bass shelf (~120 Hz)
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(120.f);
        bass.setRes(0.707f);
        bass.setLevel(0.f);

        // Treble shelf (~5.5 kHz)
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(5500.f);
        treble.setRes(0.707f);
        treble.setLevel(0.f);

        // Upper-mid nasal peak (~1.6 kHz)
        upperMid.setType(gam::PEAKING);
        upperMid.setFreq(1600.f);
        upperMid.setRes(0.6f);
        upperMid.setLevel(+3.0f);

        makeup = 0.75f;
    }

    void reset() override
    {
        bass.reset();
        treble.reset();
        upperMid.reset();
    }

    // bass01, treble01 ∈ [0,1]
    float process(float x, float bass01, float treble01)
    {
        // ----- Control law -----

        // Bass: moderate range (Vox is not bass-heavy)
        float bassDB = -8.f + 16.f * bass01;

        // Treble: very influential, also affects perceived gain
        float trebleDB = -10.f + 20.f * treble01;

        bass.setLevel(bassDB);
        treble.setLevel(trebleDB);

        // ----- Filter order (critical for Vox sound) -----
        float y = x;

        y = bass.process(y);        // bass first
        y = upperMid.process(y);   // nasal character
        y = treble.process(y);     // sparkle + bite

        return y * makeup;
    }
};
