#pragma once
#include <array>
#include <algorithm>

template <class Sample, int MaxRoutes = 32>
class ModulationRouter {
public:
    struct Route {
        Sample* target;    // pointer to DSP parameter
        Sample  depth;     // modulation depth
        Sample  offset;    // base offset
        bool    active;
    };

    ModulationRouter()
    {
        reset();
    }

    void reset()
    {
        mNumRoutes = 0;
    }

    void addRoute(Sample* target, Sample depth, Sample offset = (Sample)0)
    {
        if (mNumRoutes < MaxRoutes)
            mRoutes[mNumRoutes++] = { target, depth, offset, true };
    }

    void enableRoute(int index, bool e)
    {
        if (index >= 0 && index < mNumRoutes)
            mRoutes[index].active = e;
    }

    void process(Sample controlValue)
    {
        for (int i = 0; i < mNumRoutes; ++i)
        {
            auto& r = mRoutes[i];
            if (!r.active || !r.target) continue;

            *(r.target) = r.offset + controlValue * r.depth;
        }
    }

private:
    std::array<Route, MaxRoutes> mRoutes;
    int mNumRoutes = 0;
};
