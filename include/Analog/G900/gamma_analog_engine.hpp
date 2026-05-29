#pragma once
/*
    GDSP/Analog/G900/gamma_analog_engine.hpp

    G900 Gamma Analog Engine v0A
    Single-header C++20 analog-style modular DSP core.

    Scope v0A:
    - allocation-free audio path
    - Gamma-style domain contract via Td::spu()
    - fixed-size tiny math
    - smoothed parameters
    - VCO -> CP3 ZDF mixer -> nonlinear ladder -> OTA VCA voice

    This header intentionally does not include or modify the existing
    PatchEngine/SWIG runtime. It is a standalone C++ module layer that can be
    wired into GammaDSP later.
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#ifndef GAM_DEFAULT_DOMAIN
struct G900_DefaultDomain {
    static double spu() { return 48000.0; }
    static double ups() { return 1.0 / 48000.0; }
};
#define GAM_DEFAULT_DOMAIN G900_DefaultDomain
#endif

namespace g900 {

// ============================================================
// 00. CONFIGURATION
// ============================================================

static constexpr int G900_MAX_NEWTON_ITERS = 8;
static constexpr float G900_PARAM_EPSILON = 1.0e-8f;
static constexpr float G900_DEFAULT_SMOOTH_MS = 10.0f;

// ============================================================
// 01. UTILITY MATH
// ============================================================

template<class T>
inline T safe_div(T a, T b, T fallback = T(0))
{
    return std::abs(b) > T(1e-20) ? a / b : fallback;
}

template<class T>
inline T limit_step(T dx, T maxStep)
{
    return std::clamp(dx, -maxStep, maxStep);
}

template<class T>
inline bool finite(T x)
{
    return std::isfinite(x);
}

// ============================================================
// 02. PARAMETER SMOOTHING
// ============================================================

enum class ParamSmoothingMode {
    None,
    Linear,
    OnePole
};

template<class T=float>
struct G900Param {
    T current = T(0);
    T target = T(0);
    T step = T(0);

    T minValue = T(-1);
    T maxValue = T(1);
    T smoothMs = T(G900_DEFAULT_SMOOTH_MS);
    T onePoleA = T(0);

    int samplesLeft = 0;
    ParamSmoothingMode mode = ParamSmoothingMode::OnePole;

    void reset(T value)
    {
        value = std::clamp(value, minValue, maxValue);
        current = value;
        target = value;
        step = T(0);
        samplesLeft = 0;
        onePoleA = T(0);
    }

    void setRange(T mn, T mx)
    {
        minValue = mn;
        maxValue = mx;
        current = std::clamp(current, minValue, maxValue);
        target = std::clamp(target, minValue, maxValue);
    }

    void setImmediate(T value)
    {
        reset(value);
    }

    void setTarget(T value, T sr)
    {
        target = std::clamp(value, minValue, maxValue);

        if(mode == ParamSmoothingMode::None) {
            setImmediate(target);
            return;
        }

        if(mode == ParamSmoothingMode::Linear) {
            const int n = std::max(1, int((smoothMs * T(0.001)) * sr));
            samplesLeft = n;
            step = (target - current) / T(n);
            return;
        }

        const T tau = std::max(T(0.0001), smoothMs * T(0.001));
        onePoleA = std::exp(T(-1) / (tau * sr));
    }

    T tick()
    {
        if(mode == ParamSmoothingMode::None) {
            current = target;
            return current;
        }

        if(mode == ParamSmoothingMode::Linear) {
            if(samplesLeft > 0) {
                current += step;
                --samplesLeft;
            } else {
                current = target;
            }
            return current;
        }

        current = target + onePoleA * (current - target);
        if(std::abs(current - target) < T(G900_PARAM_EPSILON)) {
            current = target;
        }
        return current;
    }

    T value() const
    {
        return current;
    }
};

// ============================================================
// 03. TINY FIXED LINEAR ALGEBRA
// ============================================================

template<class T, int N>
struct Vec {
    T v[N]{};

    T& operator[](int i) { return v[i]; }
    const T& operator[](int i) const { return v[i]; }

    void clear()
    {
        for(int i = 0; i < N; ++i) v[i] = T(0);
    }
};

template<class T, int N>
struct Mat {
    T a[N][N]{};

    T* operator[](int i) { return a[i]; }
    const T* operator[](int i) const { return a[i]; }

    void clear()
    {
        for(int r = 0; r < N; ++r) {
            for(int c = 0; c < N; ++c) {
                a[r][c] = T(0);
            }
        }
    }
};

template<class T, int N>
inline bool solve_dense(Mat<T, N> A, Vec<T, N> b, Vec<T, N>& x)
{
    for(int i = 0; i < N; ++i) x[i] = T(0);

    for(int k = 0; k < N; ++k) {
        int pivot = k;
        T maxAbs = std::abs(A[k][k]);

        for(int r = k + 1; r < N; ++r) {
            const T candidate = std::abs(A[r][k]);
            if(candidate > maxAbs) {
                maxAbs = candidate;
                pivot = r;
            }
        }

        if(maxAbs < T(1e-20)) {
            return false;
        }

        if(pivot != k) {
            for(int c = 0; c < N; ++c) {
                std::swap(A[k][c], A[pivot][c]);
            }
            std::swap(b[k], b[pivot]);
        }

        const T diag = A[k][k];
        for(int c = k; c < N; ++c) A[k][c] /= diag;
        b[k] /= diag;

        for(int r = 0; r < N; ++r) {
            if(r == k) continue;
            const T factor = A[r][k];
            for(int c = k; c < N; ++c) {
                A[r][c] -= factor * A[k][c];
            }
            b[r] -= factor * b[k];
        }
    }

    for(int i = 0; i < N; ++i) x[i] = b[i];
    return true;
}

// ============================================================
// 04. NONLINEAR DEVICES
// ============================================================

template<class T=float>
struct DiffPairTanh {
    T Isat = T(2e-4);
    T k = T(3.5);
    T bias = T(0.02);
    T wp = T(1);
    T wn = T(1);

    inline void eval(T v, T& i, T& di) const
    {
        const T vp = k * (v + bias);
        const T vn = k * (v - bias);

        const T tp = std::tanh(vp);
        const T tn = std::tanh(vn);

        i = Isat * (wp * tp + wn * tn);
        di = Isat * k * (wp * (T(1) - tp * tp) + wn * (T(1) - tn * tn));
    }
};

// ============================================================
// 05. G942 CP3 MIXER ZDF
// ============================================================

template<class T=float, class Td=GAM_DEFAULT_DOMAIN>
class G942_CP3MixerZDF : public Td {
public:
    static constexpr int N = 4;

    T R[N] = { T(10000), T(10000), T(10000), T(10000) };
    T g[N] = {};

    T Csum = T(1e-9);
    T Rleak = T(1e6);

    G900Param<T> drive;
    G900Param<T> outputGain;
    DiffPairTanh<T> diff;

    T Vs = T(0);
    T Vz = T(0);

    G942_CP3MixerZDF()
    {
        drive.setRange(T(0), T(8));
        drive.reset(T(1));
        outputGain.setRange(T(0), T(8));
        outputGain.reset(T(1));
        recalc();
    }

    void reset()
    {
        Vs = T(0);
        Vz = T(0);
    }

    void onDomainChange(double)
    {
        recalc();
    }

    void recalc()
    {
        for(int i = 0; i < N; ++i) {
            g[i] = safe_div(T(1), std::max(R[i], T(1)), T(0));
        }
    }

    inline T operator()(T a, T b, T c, T d)
    {
        const T vin[N] = { a, b, c, d };
        return operator()(vin);
    }

    inline T operator()(const T* vin)
    {
        const T dt = T(1.0 / Td::spu());
        const T driveNow = drive.tick();
        const T outputGainNow = outputGain.tick();

        T v = Vs;

        for(int it = 0; it < 6; ++it) {
            T F = T(0);
            T J = T(0);

            for(int i = 0; i < N; ++i) {
                F += g[i] * (driveNow * vin[i] - v);
                J -= g[i];
            }

            T inl = T(0);
            T dinl = T(0);
            diff.eval(v, inl, dinl);

            F -= inl;
            J -= dinl;

            const T gleak = safe_div(T(1), std::max(Rleak, T(1)), T(0));
            F -= gleak * v;
            J -= gleak;

            F -= Csum * (v - Vz) / dt;
            J -= Csum / dt;

            T dv = safe_div(-F, J, T(0));
            dv = limit_step(dv, T(0.5));
            v += dv;

            if(std::abs(dv) < T(1e-7)) break;
        }

        if(!finite(v)) v = T(0);

        Vs = v;
        Vz = v;
        return outputGainNow * v;
    }
};

// ============================================================
// 06. G951 LADDER ZDF
// ============================================================

template<class T=float, class Td=GAM_DEFAULT_DOMAIN>
class G951_LadderZDF : public Td {
public:
    G900Param<T> cutoff;
    G900Param<T> resonance;
    G900Param<T> drive;
    G900Param<T> outGain;

    T z1 = T(0);
    T z2 = T(0);
    T z3 = T(0);
    T z4 = T(0);
    T g = T(0.1);

    G951_LadderZDF()
    {
        cutoff.setRange(T(5), T(20000));
        cutoff.reset(T(1000));
        resonance.setRange(T(0), T(4));
        resonance.reset(T(0.2));
        drive.setRange(T(0), T(8));
        drive.reset(T(1));
        outGain.setRange(T(0), T(8));
        outGain.reset(T(1));
        recalc();
    }

    void reset()
    {
        z1 = z2 = z3 = z4 = T(0);
    }

    void onDomainChange(double)
    {
        recalc();
    }

    void setCutoff(T hz) { cutoff.setTarget(hz, T(Td::spu())); }
    void setCutoffImmediate(T hz) { cutoff.setImmediate(hz); recalc(); }
    void setResonance(T r) { resonance.setTarget(r, T(Td::spu())); }
    void setDrive(T d) { drive.setTarget(d, T(Td::spu())); }

    void recalc()
    {
        recalc_from_cutoff(cutoff.value());
    }

    void recalc_from_cutoff(T cutoffHz)
    {
        const T sr = T(Td::spu());
        const T f = std::clamp(cutoffHz, T(5), sr * T(0.45));
        const T wd = T(3.1415926535897932384626433832795) * f / sr;
        const T t = std::tan(wd);
        g = t / (T(1) + t);
    }

    inline T sat(T x) const
    {
        return std::tanh(x);
    }

    inline T dsat(T x) const
    {
        const T t = std::tanh(x);
        return T(1) - t * t;
    }

    inline T operator()(T input)
    {
        const T cutoffNow = cutoff.tick();
        const T resonanceNow = resonance.tick();
        const T driveNow = drive.tick();
        const T outGainNow = outGain.tick();

        recalc_from_cutoff(cutoffNow);

        Vec<T,4> x;
        Vec<T,4> F;
        Vec<T,4> rhs;
        Vec<T,4> dx;
        Mat<T,4> J;

        x[0] = z1;
        x[1] = z2;
        x[2] = z3;
        x[3] = z4;

        for(int it = 0; it < G900_MAX_NEWTON_ITERS; ++it) {
            F.clear();
            rhs.clear();
            J.clear();

            const T fb = resonanceNow * x[3];
            const T u = driveNow * input - fb;

            const T tu = sat(u);
            const T t1 = sat(x[0]);
            const T t2 = sat(x[1]);
            const T t3 = sat(x[2]);
            const T t4 = sat(x[3]);

            const T du = dsat(u);
            const T d1 = dsat(x[0]);
            const T d2 = dsat(x[1]);
            const T d3 = dsat(x[2]);
            const T d4 = dsat(x[3]);

            F[0] = x[0] - z1 - g * (tu - t1);
            F[1] = x[1] - z2 - g * (t1 - t2);
            F[2] = x[2] - z3 - g * (t2 - t3);
            F[3] = x[3] - z4 - g * (t3 - t4);

            J[0][0] = T(1) + g * d1;
            J[0][1] = T(0);
            J[0][2] = T(0);
            J[0][3] = g * resonanceNow * du;

            J[1][0] = -g * d1;
            J[1][1] = T(1) + g * d2;
            J[1][2] = T(0);
            J[1][3] = T(0);

            J[2][0] = T(0);
            J[2][1] = -g * d2;
            J[2][2] = T(1) + g * d3;
            J[2][3] = T(0);

            J[3][0] = T(0);
            J[3][1] = T(0);
            J[3][2] = -g * d3;
            J[3][3] = T(1) + g * d4;

            for(int i = 0; i < 4; ++i) rhs[i] = -F[i];

            if(!solve_dense<T,4>(J, rhs, dx)) break;

            T maxdx = T(0);
            for(int i = 0; i < 4; ++i) {
                dx[i] = limit_step(dx[i], T(0.5));
                x[i] += dx[i];
                maxdx = std::max(maxdx, std::abs(dx[i]));
            }

            if(maxdx < T(1e-6)) break;
        }

        for(int i = 0; i < 4; ++i) {
            if(!finite(x[i])) x[i] = T(0);
        }

        z1 = x[0];
        z2 = x[1];
        z3 = x[2];
        z4 = x[3];

        return outGainNow * z4;
    }
};

// ============================================================
// 07. G960 VCA OTA
// ============================================================

template<class T=float, class Td=GAM_DEFAULT_DOMAIN>
class G960_VCA_OTA : public Td {
public:
    G900Param<T> gain;
    G900Param<T> level;

    G960_VCA_OTA()
    {
        gain.setRange(T(0), T(16));
        gain.reset(T(1));
        level.setRange(T(0), T(4));
        level.reset(T(0.25));
    }

    void reset() {}
    void onDomainChange(double) {}

    inline T operator()(T input)
    {
        const T gainNow = gain.tick();
        const T levelNow = level.tick();
        return levelNow * std::tanh(gainNow * input);
    }
};

// ============================================================
// 08. G971 VCO CORE
// ============================================================

template<class T=float, class Td=GAM_DEFAULT_DOMAIN>
class G971_VCO_Core : public Td {
public:
    enum Wave {
        SAW,
        TRIANGLE,
        PULSE,
        SINE
    };

    G900Param<T> freq;
    G900Param<T> pulseWidth;
    T phase = T(0);
    Wave wave = SAW;

    G971_VCO_Core()
    {
        freq.setRange(T(0.01), T(20000));
        freq.reset(T(110));
        pulseWidth.setRange(T(0.01), T(0.99));
        pulseWidth.reset(T(0.5));
    }

    void reset()
    {
        phase = T(0);
    }

    void onDomainChange(double) {}

    void setFreq(T hz) { freq.setTarget(hz, T(Td::spu())); }
    void setFreqImmediate(T hz) { freq.setImmediate(hz); }
    void setPulseWidth(T pw) { pulseWidth.setTarget(pw, T(Td::spu())); }

    inline T advance()
    {
        const T dt = T(1.0 / Td::spu());
        const T f = freq.tick();
        phase += f * dt;

        while(phase >= T(1)) phase -= T(1);
        while(phase < T(0)) phase += T(1);
        return phase;
    }

    inline T operator()()
    {
        const T p = advance();
        switch(wave) {
            case SAW:
                return T(2) * p - T(1);
            case TRIANGLE:
                return T(4) * std::abs(p - T(0.5)) - T(1);
            case PULSE:
                return p < pulseWidth.tick() ? T(1) : T(-1);
            case SINE:
            default:
                return std::sin(T(6.283185307179586476925286766559) * p);
        }
    }
};

// ============================================================
// 09. COMPLETE G900 VOICE
// ============================================================

template<class T=float, class Td=GAM_DEFAULT_DOMAIN>
class G900Voice : public Td {
public:
    G971_VCO_Core<T,Td> osc1;
    G971_VCO_Core<T,Td> osc2;
    G971_VCO_Core<T,Td> osc3;
    G971_VCO_Core<T,Td> osc4;

    G942_CP3MixerZDF<T,Td> mixer;
    G951_LadderZDF<T,Td> ladder;
    G960_VCA_OTA<T,Td> vca;

    G900Param<T> masterGain;

    G900Voice()
    {
        osc1.freq.setImmediate(T(110));
        osc2.freq.setImmediate(T(220));
        osc3.freq.setImmediate(T(330));
        osc4.freq.setImmediate(T(440));

        osc1.wave = G971_VCO_Core<T,Td>::SAW;
        osc2.wave = G971_VCO_Core<T,Td>::SAW;
        osc3.wave = G971_VCO_Core<T,Td>::TRIANGLE;
        osc4.wave = G971_VCO_Core<T,Td>::PULSE;

        ladder.cutoff.setImmediate(T(1200));
        ladder.resonance.setImmediate(T(0.35));
        ladder.drive.setImmediate(T(1.2));

        vca.level.setImmediate(T(0.25));
        vca.gain.setImmediate(T(1.5));

        masterGain.setRange(T(0), T(4));
        masterGain.reset(T(0.5));
    }

    void reset()
    {
        osc1.reset();
        osc2.reset();
        osc3.reset();
        osc4.reset();
        mixer.reset();
        ladder.reset();
        vca.reset();
    }

    void onDomainChange(double r)
    {
        osc1.onDomainChange(r);
        osc2.onDomainChange(r);
        osc3.onDomainChange(r);
        osc4.onDomainChange(r);
        mixer.onDomainChange(r);
        ladder.onDomainChange(r);
        vca.onDomainChange(r);
    }

    inline T operator()()
    {
        const T v[4] = { osc1(), osc2(), osc3(), osc4() };
        const T m = mixer(v);
        const T f = ladder(m);
        const T y = vca(f);
        return masterGain.tick() * y;
    }
};

} // namespace g900
