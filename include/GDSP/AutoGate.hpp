#pragma once

class AutoGate {
public:
    AutoGate();

    void reset();

    void process(float activity,
                 float noisiness,
                 float energy,
                 float tension);

    float threshold() const { return mThreshold; } // dB
    float attack() const    { return mAttack; }    // ms
    float release() const   { return mRelease; }   // ms
    float range() const     { return mRange; }     // dB
};
