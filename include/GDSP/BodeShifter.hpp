#pragma once
#include "FrequencyShifer.hpp"

class BodeShifter : public Function {
public:
    BodeShifter(float shiftHz = 100.f)
    : mShift(shiftHz),
      up( shiftHz),
      down(-shiftHz)
    {
        setLevels(1.f, 1.f);
    }

    void setShift(float hz)
    {
        mShift = hz;
        up.setShift( hz);
        down.setShift(-hz);
    }

    void setLevels(float upLevel, float downLevel)
    {
        levelUp   = upLevel;
        levelDown = downLevel;
    }

    void reset()
    {
        up.reset();
        down.reset();
    }

    float process(float x) override
    {
        float u = up.process(x);
        float d = down.process(x);
        return u * levelUp + d * levelDown;
    }

private:
    float mShift;
    float levelUp   = 1.f;
    float levelDown = 1.f;

    FrequencyShifter up;
    FrequencyShifter down;
};
