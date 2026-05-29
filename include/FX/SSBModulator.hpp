#pragma once
#include "BiquadFrequencyShifer.hpp"

class SSBModulator : public Function {
public:
    enum Mode { UPPER, LOWER };

    SSBModulator(float shiftHz = 100.f, Mode m = UPPER)
    : mode(m),
      up( shiftHz),
      down(-shiftHz)
    {}

    void setShift(float hz)
    {
        up.setShift( hz);
        down.setShift(-hz);
    }

    void setMode(Mode m) { mode = m; }

    void reset()
    {
        up.reset();
        down.reset();
    }

    float process(float x) override
    {
        if (mode == UPPER)
            return up.process(x);
        else
            return down.process(x);
    }

private:
    Mode mode;
    FrequencyShifter up;
    FrequencyShifter down;
};
