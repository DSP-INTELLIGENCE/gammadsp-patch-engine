#pragma once
#include <array>
#include <algorithm>
#include <cmath>

template<class T>
struct BiquadCoefs {
    // Gamma-style: feedforward a0,a1,a2 and feedback b1,b2
    T a0{}, a1{}, a2{}, b1{}, b2{};
};

template<class T>
struct BiquadState {
    T x1{}, x2{}, y1{}, y2{};
    void reset() { x1=x2=y1=y2=T(0); }
};

template<class T>
inline T biquad_process(const BiquadCoefs<T>& c, BiquadState<T>& s, T x)
{
    // Direct Form I
    T y = c.a0*x + c.a1*s.x1 + c.a2*s.x2 - c.b1*s.y1 - c.b2*s.y2;
    s.x2 = s.x1; s.x1 = x;
    s.y2 = s.y1; s.y1 = y;
    return y;
}

// One Z-plane "model frame": an SOS cascade
template<class T, int MAX_SECTIONS>
struct ZPlaneModel {
    int sections = 0;
    std::array<BiquadCoefs<T>, MAX_SECTIONS> sec{};
};

template<class Sample, int NX, int NY, int MAX_SECTIONS>
class ZPlaneFilter : public ModFilter {
public:
    using Bank  = ZPlaneBank<Sample, NX, NY, MAX_SECTIONS>;
    using Model = ZPlaneModel<Sample, MAX_SECTIONS>;

    explicit ZPlaneFilter(const Bank* bank = nullptr)
    : mBank(bank)
    {
        // baseCutoff/baseRes used as morph coordinates
        baseCutoff = Sample(0); // morph X
        baseRes    = Sample(0); // morph Y
        setAM(Sample(1));
        reset();
    }

    void setBank(const Bank* bank) { mBank = bank; }

    // Optional: treat AM as wet level, and provide explicit dry/wet
    void setWet(Sample w) { mWet = std::clamp(w, Sample(0), Sample(1)); }

    // Smoothing (ms) using your ParamLinear style
    void setSmoothingMs(float msX, float msY) {
        mXSmooth.setTime(msX);
        mYSmooth.setTime(msY);
    }

    void reset()
    {
        for (auto& st : mState) st.reset();
        mLastSections = 0;
        mXSmooth = ParamLinear(0.f, 30.f);
        mYSmooth = ParamLinear(0.f, 30.f);
        mX = mY = Sample(0);
    }

    float process(float input) override
    {
        if (!mBank) return input * am.process();

        // 1) morph coordinates from ModFilter
        Sample x = Sample(baseCutoff) + Sample(0.5) * Sample(cutoff.process());
        Sample y = Sample(baseRes)    + Sample(0.5) * Sample(res.process());
        x = std::clamp(x, Sample(0), Sample(1));
        y = std::clamp(y, Sample(0), Sample(1));

        // 2) smooth coordinates (zipper-free morph)
        mXSmooth.set((float)x);
        mYSmooth.set((float)y);
        mX = (Sample)mXSmooth.process();
        mY = (Sample)mYSmooth.process();

        // 3) compute coefficient set for this (x,y)
        computeInterpolatedCoefs(mX, mY);

        // 4) run SOS cascade
        Sample in = (Sample)input;
        Sample z  = in;
        for (int i = 0; i < mLastSections; ++i) {
            z = biquad_process(mCur.sec[i], mState[i], z);
        }

        // 5) wet/dry + AM
        Sample out = (Sample(1) - mWet) * in + mWet * z;
        out *= (Sample)am.process();
        return (float)out;
    }

private:
    void computeInterpolatedCoefs(Sample x01, Sample y01)
    {
        // Map [0..1] to grid coordinates [0..NX-1], [0..NY-1]
        Sample gx = x01 * Sample(NX - 1);
        Sample gy = y01 * Sample(NY - 1);

        int x0 = (int)std::floor((double)gx);
        int y0 = (int)std::floor((double)gy);

        int x1 = std::min(x0 + 1, NX - 1);
        int y1 = std::min(y0 + 1, NY - 1);

        Sample tx = gx - Sample(x0);
        Sample ty = gy - Sample(y0);

        const Model& A = mBank->at(x0, y0);
        const Model& B = mBank->at(x1, y0);
        const Model& C = mBank->at(x0, y1);
        const Model& D = mBank->at(x1, y1);

        // For safety, use the minimum section count present
        int nSec = std::min({A.sections, B.sections, C.sections, D.sections, MAX_SECTIONS});
        mCur.sections = nSec;
        mLastSections = nSec;

        // Bilinear interpolate each coefficient
        for (int i = 0; i < nSec; ++i) {
            mCur.sec[i] = bilerp(A.sec[i], B.sec[i], C.sec[i], D.sec[i], tx, ty);
            stabilize(mCur.sec[i]);
        }

        // If section count changed, reset unused states (optional)
        for (int i = nSec; i < MAX_SECTIONS; ++i) {
            mState[i].reset();
        }
    }

    static BiquadCoefs<Sample> bilerp(
        const BiquadCoefs<Sample>& A,
        const BiquadCoefs<Sample>& B,
        const BiquadCoefs<Sample>& C,
        const BiquadCoefs<Sample>& D,
        Sample tx, Sample ty)
    {
        // lerp helper
        auto L = [](Sample a, Sample b, Sample t) { return a + (b - a) * t; };

        BiquadCoefs<Sample> AB, CD, out;

        AB.a0 = L(A.a0, B.a0, tx);  AB.a1 = L(A.a1, B.a1, tx);  AB.a2 = L(A.a2, B.a2, tx);
        AB.b1 = L(A.b1, B.b1, tx);  AB.b2 = L(A.b2, B.b2, tx);

        CD.a0 = L(C.a0, D.a0, tx);  CD.a1 = L(C.a1, D.a1, tx);  CD.a2 = L(C.a2, D.a2, tx);
        CD.b1 = L(C.b1, D.b1, tx);  CD.b2 = L(C.b2, D.b2, tx);

        out.a0 = L(AB.a0, CD.a0, ty); out.a1 = L(AB.a1, CD.a1, ty); out.a2 = L(AB.a2, CD.a2, ty);
        out.b1 = L(AB.b1, CD.b1, ty); out.b2 = L(AB.b2, CD.b2, ty);

        return out;
    }

    static void stabilize(BiquadCoefs<Sample>& c)
    {
        // Very lightweight safety clamps.
        // True pole/zero stability enforcement is heavier; this is the pragmatic approach.
        // Keep feedback coefficients bounded (typical stable IIR ranges)
        c.b1 = std::clamp(c.b1, Sample(-1.999), Sample(1.999));
        c.b2 = std::clamp(c.b2, Sample(-0.999), Sample(0.999));

        // Prevent denorm-ish tiny coefficients from blowing noise
        if (std::abs((double)c.a0) < 1e-20) c.a0 = Sample(0);
        if (std::abs((double)c.a1) < 1e-20) c.a1 = Sample(0);
        if (std::abs((double)c.a2) < 1e-20) c.a2 = Sample(0);
    }

private:
    const Bank* mBank = nullptr;

    // Current interpolated model
    Model mCur{};

    // Per-section state (no heap)
    std::array<BiquadState<Sample>, MAX_SECTIONS> mState{};

    int mLastSections = 0;

    // morph smoothing uses your ParamLinear class (float-based)
    ParamLinear mXSmooth{0.f, 30.f};
    ParamLinear mYSmooth{0.f, 30.f};
    Sample mX = Sample(0);
    Sample mY = Sample(0);

    Sample mWet = Sample(1);
};
