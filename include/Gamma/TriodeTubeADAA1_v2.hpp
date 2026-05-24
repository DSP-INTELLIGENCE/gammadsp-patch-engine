#pragma once
#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ============================================================
// Curve: Triode tanh mix (pluggable)
// ============================================================
template<typename T>
struct TriodeCurveTanhMix {
	static_assert(std::is_floating_point_v<T>,
		"TriodeCurveTanhMix<T>: T must be float/double");

	struct Params {
		T k;     // curvature (>0)
		T bias;  // operating-point offset
		T asym;  // [-1,1] branch weighting
	};

	// log(cosh(z)) = |z| + log1p(exp(-2|z|)) - log(2)
	static inline T logcosh(T z){
		const T az = std::abs(z);
		return az + std::log1p(std::exp(T(-2) * az))
		         - T(0.6931471805599453094); // log(2)
	}

	static inline T transfer(T x, const Params& p){
		const T wp = T(0.5) * (T(1) + p.asym);
		const T wm = T(0.5) * (T(1) - p.asym);
		return wp * std::tanh(p.k * (x + p.bias))
		     + wm * std::tanh(p.k * (x - p.bias));
	}

	static inline T primitive(T x, const Params& p){
		const T wp = T(0.5) * (T(1) + p.asym);
		const T wm = T(0.5) * (T(1) - p.asym);
		return (wp * logcosh(p.k * (x + p.bias))
		      + wm * logcosh(p.k * (x - p.bias))) / p.k;
	}
};


/// ============================================================
/// TriodeTubeADAA1_v2 (Gamma-native)
///
/// • Exact ADAA1 (primitive difference quotient)
/// • Compile-time pluggable curve
/// • Calibration-correct
/// • Gamma lifecycle: reset(), operator(), onDomainChange()
/// ============================================================
template<
	typename T = gam::real,
	template<typename> class Curve = TriodeCurveTanhMix,
	class Td = GAM_DEFAULT_DOMAIN
>
class TriodeTubeADAA1_v2 : public Td {
	static_assert(std::is_floating_point_v<T>,
		"TriodeTubeADAA1_v2<T>: T must be float/double");

public:
	using CurveT = Curve<T>;
	using Params = typename CurveT::Params;

	TriodeTubeADAA1_v2(){
		reset();
		onDomainChange(1.0); // ensure domain-dependent init even when standalone
	}

	void reset(){
		mPrevX = T(0);
		mHasPrev = false;
	}

	void onDomainChange(double){
		// currently nothing SR-dependent inside; left intentionally for future hooks
	}

	// Parameters
	void drive(T v)       { mDrive = std::max(v, T(0)); }
	void saturation(T v)  { mSat01 = scl::clip(v, T(0), T(1)); }
	void bias(T v)        { mBias  = v; }
	void asymmetry(T v)   { mAsym  = scl::clip(v, T(-1), T(1)); }
	void trim(T v)        { mTrim  = v; }

	void calibration(T ref){
		mRef = std::max(ref, kRefMin);
	}

	void maxCurvature(T kMax){
		mKMax = std::max(kMax, kMin());
	}

	// Processing
	inline T operator()(T x){
		if(!std::isfinite(x)) return T(0);

		// normalize + pre-gain
		T xn = (x / mRef) * mDrive;

		// params snapshot (ADAA requirement)
		Params p;
		p.k    = curvatureFromSat(mSat01);
		p.bias = mBias;
		p.asym = mAsym;

		// ADAA1
		T y = adaa1(xn, p);

		// de-normalize
		return (y * mTrim) * mRef;
	}

	// Debug helpers
	T transfer(T x) const { return CurveT::transfer(x, currentParams()); }
	T primitive(T x) const { return CurveT::primitive(x, currentParams()); }

private:
	static constexpr T kRefMin = T(1e-6);
	static constexpr T kMinV   = T(1e-6);

	static constexpr T kMin(){ return kMinV; }

	// sat ∈ [0,1] → curvature k
	T curvatureFromSat(T s) const {
		const T kLo = kMin();
		const T kHi = std::max(mKMax, kLo);

		if(s <= T(0)) return kLo;
		if(s >= T(1)) return kHi;

		const T a = T(6); // perceptual steepness
		const T num = std::exp2(a * s) - T(1);
		const T den = std::exp2(a)     - T(1);
		const T t   = num / den;

		return kLo + (kHi - kLo) * t;
	}

	Params currentParams() const {
		Params p;
		p.k    = curvatureFromSat(mSat01);
		p.bias = mBias;
		p.asym = mAsym;
		return p;
	}

	static T scaledEps(T x, T xPrev){
		const T C = T(16);
		return C * std::numeric_limits<T>::epsilon()
		     * (T(1) + std::abs(x) + std::abs(xPrev));
	}

	// ADAA1 core
	T adaa1(T x, const Params& p){
		if(!mHasPrev){
			mPrevX = x;
			mHasPrev = true;
			return CurveT::transfer(x, p);
		}

		const T xPrev = mPrevX;
		mPrevX = x;

		const T dx  = x - xPrev;
		const T eps = scaledEps(x, xPrev);

		if(std::abs(dx) <= eps){
			return CurveT::transfer(T(0.5) * (x + xPrev), p);
		}

		const T Fx  = CurveT::primitive(x, p);
		const T Fxp = CurveT::primitive(xPrev, p);
		const T y   = (Fx - Fxp) / dx;

		return std::isfinite(y) ? y : T(0);
	}

private:
	// state
	T    mPrevX   = T(0);
	bool mHasPrev = false;

	// params
	T mDrive = T(1);
	T mSat01 = T(0.5);
	T mBias  = T(0);
	T mAsym  = T(0);
	T mTrim  = T(1);
	T mRef   = T(1);
	T mKMax  = T(16);
};

} // namespace gam
