#pragma once
#include <algorithm>

template <class Sample>
class SignalProfiler {
public:
    struct Profile {
        Sample meanEnergy     = (Sample)0;
        Sample meanBrightness = (Sample)0;
        Sample meanMotion     = (Sample)0;
        Sample meanStability  = (Sample)0;

        Sample dynamicRange   = (Sample)0;
        Sample complexity     = (Sample)0;
        Sample tonalness      = (Sample)0;
    };

    SignalProfiler()
    {
        reset();
    }

    void reset()
    {
        mProfile = {};
    }

    // Feed long-term descriptors (0..1 normalized)
    Profile process(Sample energy, Sample brightness, Sample flux, Sample stability)
    {
        // Ultra-slow learning
        mProfile.meanEnergy     = (Sample)0.999 * mProfile.meanEnergy     + (Sample)0.001 * energy;
        mProfile.meanBrightness = (Sample)0.999 * mProfile.meanBrightness + (Sample)0.001 * brightness;
        mProfile.meanMotion     = (Sample)0.999 * mProfile.meanMotion     + (Sample)0.001 * flux;
        mProfile.meanStability  = (Sample)0.999 * mProfile.meanStability  + (Sample)0.001 * stability;

        // Derived descriptors
        mProfile.dynamicRange = std::clamp(energy - mProfile.meanEnergy + (Sample)0.5, (Sample)0, (Sample)1);
        mProfile.complexity   = std::clamp(flux * (Sample)0.7 + brightness * (Sample)0.3, (Sample)0, (Sample)1);
        mProfile.tonalness    = std::clamp(stability * (Sample)0.8 + (Sample)0.2, (Sample)0, (Sample)1);

        return mProfile;
    }

    const Profile& profile() const { return mProfile; }

private:
    Profile mProfile;
};
