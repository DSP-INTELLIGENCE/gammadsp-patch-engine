#pragma once
#include <algorithm>

template <class Sample>
class HoldGenerator {
public:
    void setHold(Sample seconds)
    {
        holdSamples = (int)(std::max((Sample)0, seconds) * gam::sampleRate());
    }

    void reset()
    {
        counter = 0;
    }

    // Returns true if we are currently holding
    bool process(bool releasing)
    {
        if (!releasing)
        {
            // Compression still increasing → reset hold
            counter = holdSamples;
            return false;
        }

        if (counter > 0)
        {
            --counter;
            return true;
        }

        return false;
    }

private:
    int holdSamples = 0;
    int counter = 0;
};
