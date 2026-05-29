#pragma once
#include <algorithm>

template <class Sample>
class SidechainMonitor {
public:
    void enable(bool b) { enabled = b; }
    bool isEnabled() const { return enabled; }

    // Gain for the monitor feed
    void setLevel(Sample g) { gain = g; }

    // Mixes sidechain signal into output if enabled
    inline void process(Sample sc, Sample& outL, Sample& outR)
    {
        if (!enabled) return;

        Sample s = sc * gain;
        outL = s;
        outR = s;
    }

private:
    bool enabled { false };
    Sample gain { (Sample)1 };
};
