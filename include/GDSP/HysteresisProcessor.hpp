#pragma once
#include <algorithm>

template <class Sample>
class HysteresisProcessor {
public:
    void setAmount(Sample a)
    {
        amount = std::clamp(a, (Sample)0, (Sample)0.999);
    }

    void reset(Sample v = 0)
    {
        state = v;
    }

    // Applies hysteresis to a control signal
    inline Sample process(Sample input)
    {
        state = state * amount + input * ((Sample)1 - amount);
        return state;
    }

private:
    Sample amount { 0 };
    Sample state  { 0 };
};
