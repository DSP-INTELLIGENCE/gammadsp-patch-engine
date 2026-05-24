#pragma once
#include <array>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

// assumes Filter has: set(freq, Q), reset(v), operator()(x)

template<unsigned ORDER>
struct ButterworthQ {
    static_assert(ORDER >= 2 && (ORDER % 2) == 0);
    static constexpr unsigned stages = ORDER / 2;

    static constexpr std::array<double, stages> values() {
        std::array<double, stages> q{};
        for (unsigned k = 0; k < stages; ++k) {
            q[k] = 1.0 / (2.0 * std::sin((2.0 * k + 1.0) * M_PI / (2.0 * ORDER)));
        }
        return q;
    }
};

namespace gam {

template<
    unsigned ORDER,
    class Filter,
    class T = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class LinkwitzRileyLP : public Td {
public:
    static_assert(ORDER >= 4 && (ORDER % 4) == 0,
                  "Linkwitz-Riley order must be a multiple of 4 (4,8,12,...)");

    static constexpr unsigned halfOrder = ORDER / 2;          // Butterworth half
    static constexpr unsigned halfStages = halfOrder / 2;     // biquads in half
    static constexpr unsigned stages = ORDER / 2;             // total biquads

    LinkwitzRileyLP(T freqHz = T(1000)) { setFreq(freqHz); }

    void reset(T v = T(0)){
        for(auto& s : mStages) s.reset(v);
    }

    void setFreq(T f){
        mFreq = f;
        updateStages();
    }

    inline T operator()(T x){
        for(auto& s : mStages) x = s(x);
        return x;
    }

    void onDomainChange(double){
        updateStages();
    }

private:
    T mFreq = T(1000);
    std::array<Filter, stages> mStages;

    void updateStages(){
        // Qs for the Butterworth half (ORDER/2)
        constexpr auto Qs = ButterworthQ<halfOrder>::values();

        // first half
        for(unsigned i=0; i<halfStages; ++i)
            mStages[i].set(mFreq, T(Qs[i]));

        // second half = identical copy
        for(unsigned i=0; i<halfStages; ++i)
            mStages[i + halfStages].set(mFreq, T(Qs[i]));
    }
};

} // namespace gam
