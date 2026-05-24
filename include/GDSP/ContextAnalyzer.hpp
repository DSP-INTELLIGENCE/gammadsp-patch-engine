#pragma once
#include <algorithm>

template <class Sample>
class ContextAnalyzer {
public:
    struct Context {
        Sample energy      = (Sample)0;
        Sample motion      = (Sample)0;
        Sample brightness  = (Sample)0;
        Sample stability   = (Sample)0;
        Sample density     = (Sample)0;

        bool   intense     = false;
        bool   calm        = false;
        bool   chaotic     = false;
        bool   focused     = false;
        bool   building    = false;
    };

    ContextAnalyzer()
    {
        reset();
    }

    void reset()
    {
        mCtx = {};
    }

    // Feed perception signals (0..1 normalized)
    Context process(Sample energy, Sample flux, Sample brightness, Sample stability)
    {
        // Slow integrators
        mCtx.energy     = (Sample)0.98 * mCtx.energy     + (Sample)0.02 * energy;
        mCtx.motion     = (Sample)0.95 * mCtx.motion     + (Sample)0.05 * flux;
        mCtx.brightness = (Sample)0.97 * mCtx.brightness + (Sample)0.03 * brightness;
        mCtx.stability  = (Sample)0.96 * mCtx.stability  + (Sample)0.04 * stability;

        // Derived descriptors
        mCtx.density = std::clamp(mCtx.energy * (Sample)0.6 + mCtx.motion * (Sample)0.4,
                                  (Sample)0, (Sample)1);

        mCtx.intense  = mCtx.energy   > (Sample)0.7;
        mCtx.calm     = mCtx.energy   < (Sample)0.3;
        mCtx.chaotic  = mCtx.motion   > (Sample)0.6 && mCtx.stability < (Sample)0.4;
        mCtx.focused  = mCtx.stability> (Sample)0.7 && mCtx.motion < (Sample)0.4;

        // Structural sense
        static Sample prevEnergy = (Sample)0;
        mCtx.building = (mCtx.energy > prevEnergy);
        prevEnergy = mCtx.energy;

        return mCtx;
    }

    const Context& context() const { return mCtx; }

private:
    Context mCtx;
};
