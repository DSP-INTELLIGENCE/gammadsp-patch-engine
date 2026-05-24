#pragma once
#include <cmath>
#include "SVF.hpp"   // your existing 2-pole SVF (TPT) implementation
#include "TVSVF.hpp"

class SVFLadder4
{
public:
    SVFLadder4();

    enum LadderMode
    {
        Classic_LP4,        // Moog / CAT
        Smooth_HP4,         // musical high-pass ladder
        BandPass_4,         // tight resonant band-pass
        Formant_1,          // vowel-like
        Formant_2,          // sharper vocal
        Phaser_4,           // 4-pole phaser
        Growl,              // aggressive synth tone
        Hollow,             // nasal / thin
        Notch_4,            // sweeping notch ladder
        Peak_4,             // resonant emphasis
        Experimental_1,
        Experimental_2
    };
    void setLadderMode(LadderMode mode);
    
    enum LadderType {
        MOOG_LIKE,
        CAT_LIKE,
        BUTTER_LIKE,
        BESSEL_LIKE,
        CHEBY_LIKE
    };

    void setType(LadderType type);
    void setSampleRate(float fs);
    void setCutoff(float hz);
    void enableGainCompensation(bool enabled);
    void setAnalogFeel(float amount);  // 0 = none, 1 = subtle, 2+ = very alive
    void setCutoffSaturation(float amount);

    void enableAutoTrim(bool enabled);
    void enablePitchStabilizer(bool enabled);

    void setBaseNote(float midiNote);      // pitch center
    void setKeyTracking(float amount);     // 0..1
    void setNote(float midiNote);          // current played key


    // Damping parameter r >= 0  (Moog = 1.0, CAT ≈ 1.064, Butterworth ≈ 0.707)
    void setDamping(float r);

    // Normalized resonance k̂ in [0,1)
    void setFeedback(float kHat);
    void setDrive(float amount);  // 0 = clean, 1 = warm, 2+ = aggressive
    void setStageModes(SVFType stage1, SVFType stage2);

    float process(float x, int ch);
    void reset();
    
    void updateInternal();

    float sampleRate = 44100.0f;
    float cutoff     = 1000.0f;

    float r          = 1.0f;     // SVF damping
    float kHat       = 0.0f;     // global feedback

    TVSVF svf1, svf2;

    float yLast[2] = {0,0};
    bool useGainComp = true;
    float drive = 0.0f;
    float analogFeel = 0.0f;

    float drift = 0.0f;
    float driftVel = 0.0f;
    float cutoffSatAmount = 0.0f;  // 0 = off, 1 = strong analog effect
    
    float driveScale = 1.0f;
    float resScale   = 1.0f;
    float trimTarget = 0.2f;  // desired RMS-ish level
    bool autoTrimEnabled = true;

    float trimGain = 1.0f;
    bool pitchStabilizerOn = true;

    // tracking state
    float lastOutPS[2] = {0,0};
    int   samplesSinceCross[2] = {0,0};
    float measuredFreq[2] = {0,0};
    float pitchCorrection = 1.0f;
    float baseNote = 60.0f;     // Middle C
    float currentNote = 60.0f;
    float keyTrack = 1.0f;     // 1 = perfect tracking
    bool  bMidiSync = false;    // true = attached to midi/keyboard
};

