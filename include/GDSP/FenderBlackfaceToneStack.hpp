#pragma once
#include "Biquad.hpp"
#include "ToneStack.hpp"

struct FenderBlackfaceToneStack : public ToneStack
{
    // --- Core filters ---
    Biquad bass;      // low shelf
    Biquad midCut;    // mid scoop (always active)
    Biquad midFill;   // mid control
    Biquad treble;   // high shelf

    float makeup = 0.45f;   // Fender stacks lose LOTS of level

    void prepare(float sr) override
    {
        // Bass shelf (~100 Hz)
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(100.f);
        bass.setRes(0.707f);
        bass.setLevel(0.f);

        // Fixed mid scoop (~400 Hz)
        midCut.setType(gam::PEAKING);
        midCut.setFreq(400.f);
        midCut.setRes(0.5f);
        midCut.setLevel(-10.0f);   // always-on scoop

        // Mid control (~650 Hz)
        midFill.setType(gam::PEAKING);
        midFill.setFreq(650.f);
        midFill.setRes(0.8f);
        midFill.setLevel(0.f);

        // Treble shelf (~4 kHz)
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(4000.f);
        treble.setRes(0.707f);
        treble.setLevel(0.f);

        makeup = 0.45f;
    }

    void reset() override
    {
        bass.reset();
        midCut.reset();
        midFill.reset();
        treble.reset();
    }

    // bass01, mid01, treble01 ∈ [0,1]
    float process(float x, float bass01, float mid01, float treble01)
    {
        // ----- Control law -----

        // Bass: huge influence (±14 dB)
        float bassDB = -14.f + 28.f * bass01;

        // Mid knob mostly *fills in* the scoop
        // 0 = deep scoop, 1 = almost flat mids
        float midDB = -6.f + 12.f * mid01;

        // Treble: glassy top end
        float trebDB = -12.f + 24.f * treble01;

        bass.setLevel(bassDB);
        midFill.setLevel(midDB);
        treble.setLevel(trebDB);

        // ----- Order matters (Fender topology) -----
        float y = x;
        y = bass.process(y);      // bass first
        y = midCut.process(y);    // fixed scoop
        y = midFill.process(y);   // mid recovery
        y = treble.process(y);    // sparkle last

        return y * makeup;
    }
};
