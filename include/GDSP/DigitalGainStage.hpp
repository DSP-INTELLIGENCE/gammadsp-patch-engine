#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class DigitalGainStage {
public:
    void setMakeupDb(Sample db)
    {
        makeup = std::pow((Sample)10, db / (Sample)20);
    }

    void setDryWet(Sample w)
    {
        wet = std::clamp(w, (Sample)0, (Sample)1);
    }

    void reset() {}

    inline Sample process(Sample input, Sample controlGain)
    {
        Sample y = input * controlGain * makeup;
        return ((Sample)1 - wet) * input + wet * y;
    }

private:
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };
};
