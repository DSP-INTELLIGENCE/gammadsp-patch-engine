#pragma once
#include <array>
#include <cmath>

template<unsigned ORDER>
struct ButterworthQ {
    static_assert(ORDER >= 2 && (ORDER % 2) == 0,
                  "Butterworth order must be even and >= 2");

    static constexpr unsigned stages = ORDER / 2;

    static constexpr std::array<double, stages> values() {
        std::array<double, stages> q{};
        for (unsigned k = 0; k < stages; ++k) {
            q[k] = 1.0 /
                (2.0 * std::sin(
                    (2.0 * k + 1.0) * M_PI / (2.0 * ORDER)
                ));
        }
        return q;
    }
};

template<
    unsigned ORDER,
    class Filter,
    class T = gam::real,
    class Td = GAM_DEFAULT_DOMAIN
>
class QFilter : public Td {
public:
    static_assert(ORDER >= 2 && (ORDER % 2) == 0,
                  "Butterworth order must be even");

    static constexpr unsigned stages = ORDER / 2;

    QFilter(T freqHz = T(1000)) {
        setFreq(freqHz);
    }

    void reset(T v = T(0)) {
        for (auto& s : mStages)
            s.reset(v);
    }

    void setFreq(T f) {
        mFreq = f;
        updateStages();
    }

    inline T operator()(T x) {
        for (auto& s : mStages)
            x = s(x);
        return x;
    }

    void onDomainChange(double) {
        updateStages();
    }

private:
    T mFreq = T(1000);
    std::array<Filter, stages> mStages;

    void updateStages() {
        constexpr auto Qs = ButterworthQ<ORDER>::values();
        for (unsigned i = 0; i < stages; ++i) {
            mStages[i].set(mFreq, T(Qs[i]));
        }
    }
};
