#pragma once
#include <algorithm>
#include <vector>
#include <cmath>

template <class Sample>
class Sidechain {
public:
    void setFilter(Function* f) { filter = f; }
    void useExternalInput(bool b) { external = b; }
    void setBlend(Sample m) { mix = std::clamp(m, (Sample)0, (Sample)1); }

    void setAttack(Sample ms)  { env.setAttack(ms); }
    void setRelease(Sample ms) { env.setRelease(ms); }

    Sample process(Sample in, Sample ext)
    {
        Sample sc;

        if (external)
            sc = ext;
        else
            sc = (Sample)1 - mix * in + mix * ext;  // ducking-style blend

        if (filter) sc = filter->process(sc);

        return env.process(sc);
    }

    Sample value() const { return env.value(); }

private:
    Function* filter = nullptr;     // non-owning
    EnvelopeFollower<Sample> env;

    bool  external = false;
    Sample mix = 0;
};

