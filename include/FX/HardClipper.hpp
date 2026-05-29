#pragma once
#include <algorithm>

template <class Sample>
class HardClipper {
public:
    void setCeiling(Sample c) { ceiling = std::clamp(c, (Sample)0, (Sample)1); }
    void setMix(Sample m)     { mix = std::clamp(m, (Sample)0, (Sample)1); }

    void reset() {}

    inline Sample process(Sample x)
    {
        Sample y = std::clamp(x, -ceiling, ceiling);
        return ((Sample)1 - mix) * x + mix * y;
    }

private:
    Sample ceiling { (Sample)1 };
    Sample mix     { (Sample)1 };
};
