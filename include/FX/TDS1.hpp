#include "TEngine.hpp"
#include "TParameters.hpp"
#include "TFX.hpp"
#include "TOnePoleLP.hpp"
#include "TOnePoleHP.hpp"

template<typename T>
inline T abelSaturate(T x, T Vc, T n) noexcept {
    // y = x / (1 + |x/Vc|^n)^(1/n)
    const T ax = std::abs(x);
    const T k  = std::max(T(1e-6), Vc);
    const T nn = std::max(T(0.5),  n);

    // guard: if x == 0, return 0 fast (optional micro-opt)
    if (ax == T(0)) return T(0);

    const T t  = std::pow(ax / k, nn);
    const T d  = std::pow(T(1) + t, T(1) / nn);
    return x / d;
}


template<typename T>
class TDS1: public TFunction<T> {
public:
    explicit TDS1() noexcept {
        setDrive(T(0.5));
        setTone (T(0.5));
        setLevel(T(0.5));
        inHP.freq(T(70));
        postLP.freq(T(12000));
        updateToneFilters_();
        lastTone01 = tone01;
    }

    inline T process(T x) noexcept override final {
        // update tone only if knob moved
        if (std::abs(tone01 - lastTone01) > T(1e-4)) {
            lastTone01 = tone01;
            updateToneFilters_();
        }

        const T drive_dB = T(60) * (drive01 * drive01);
        const T preGain  = std::pow(T(10), (drive_dB * T(0.05)));

        const T level_dB = T(-24) + T(30) * level01;
        const T outGain  = std::pow(T(10), (level_dB * T(0.05)));

        const T u = inHP.process(x + T(1e-20)); // denormal guard
        const T v = u * preGain;

        const T clipped = abelSaturate(v, T(0.35), T(2.8));
        const T shaped  = postLP.process(clipped);

        const T dark   = toneLP.process(shaped);
        const T bright = toneHP.process(shaped);

        const T y = (T(1) - tone01) * dark + tone01 * bright;
        return y * outGain;
    }

    inline void setDrive(T v) noexcept { drive01 = v; }
    inline void setTone (T v) noexcept { tone01  = v; }
    inline void setLevel(T v) noexcept { level01 = v; }

private:
    inline void updateToneFilters_() noexcept {
        const T t = tone01;

        const T lpBase = T(1200);
        const T lpSpan = std::log10(T(10000) / lpBase);     // decades span
        const T lpHz   = lpBase * std::pow(T(10), lpSpan * t);

        const T hpBase = T(400);
        const T hpSpan = std::log10(T(2200) / hpBase);
        const T hpHz   = hpBase * std::pow(T(10), hpSpan * t);

        toneLP.freq(lpHz);
        toneHP.freq(hpHz);
    }


private:
    T drive01 { T(0.5) };
    T tone01  { T(0.5) };
    T level01 { T(0.5) };
    T lastTone01 { T(-1) };

    TOnePoleHP<T> inHP;
    TOnePoleLP<T> toneLP;
    TOnePoleHP<T> toneHP;
    TOnePoleLP<T> postLP;
};
