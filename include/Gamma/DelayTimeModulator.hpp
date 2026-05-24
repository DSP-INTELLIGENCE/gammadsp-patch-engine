#pragma once
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

/// DelayTimeModulator
///
/// A small utility that generates a *smoothed* delay-time modulation signal
/// (in seconds or samples) suitable for clean digital delay / chorus-style
/// modulation without zipper noise.
///
/// Features:
/// - LFO shapes: sine / triangle / random-smooth
/// - Depth + rate
/// - One-pole smoothing on the output (critical for delay modulation)
/// - Slew limiting (optional) to cap instantaneous delay jumps
///
/// Output is an *offset* (add to base delay), not an absolute delay.
///
/// Gamma-native: Td-aware, operator().
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DelayTimeModulator : public Td {
public:
	enum Shape {
		SINE = 0,
		TRI  = 1,
		RAND = 2
	};

	DelayTimeModulator(){
		reset();
		rate(T(0.25));     // Hz
		depthMs(T(0));     // ms
		shape(SINE);
		smoothMs(T(10));   // ms smoothing on output offset
		slewMsPerSec(T(0)); // off
	}

	void reset(){
		mPhase = T(0);
		mOut = T(0);
		mTarget = T(0);
		mNoiseA = T(0);
		mNoiseB = T(0);
		mNoiseT = T(1); // force refresh on first tick
	}

	// ---- user params ----

	void shape(Shape s){ mShape = s; }

	/// LFO rate in Hz
	void rate(T hz){
		mRate = scl::max(hz, T(0));
	}

	/// Depth in milliseconds (peak, bipolar). Output offset in seconds.
	void depthMs(T ms){
		mDepthSec = scl::max(ms, T(0)) * T(0.001);
	}

	/// Depth in *seconds* (peak, bipolar)
	void depthSec(T sec){
		mDepthSec = scl::max(sec, T(0));
	}

	/// Output smoothing time constant in ms (one-pole)
	void smoothMs(T ms){
		mSmoothSec = scl::max(ms, T(0)) * T(0.001);
		recalcSmooth();
	}

	/// Slew limit in ms/sec (0 disables). Caps how fast the offset can move.
	void slewMsPerSec(T msPerSec){
		mSlewSecPerSec = scl::max(msPerSec, T(0)) * T(0.001);
	}

	/// Random-smooth update rate (Hz). Higher = more “wobbly”
	void randomRate(T hz){
		mRandRate = scl::max(hz, T(0.001));
	}

	// ---- getters ----
	T rate() const { return mRate; }
	T depthSec() const { return mDepthSec; }
	T smoothSec() const { return mSmoothSec; }
	T value() const { return mOut; }

	void onDomainChange(double){
		recalcSmooth();
	}

	/// Generate next modulation offset (seconds)
	inline T operator()(){
		// advance phase
		const T dt = T(Td::ups());            // seconds per sample
		const T phInc = mRate * dt;           // cycles per sample
		mPhase += phInc;
		// wrap phase into [0,1)
		if(mPhase >= T(1)) mPhase -= scl::floor(mPhase);

		// compute raw LFO in [-1, 1]
		T lfo = T(0);
		switch(mShape){
			default:
			case SINE:
				lfo = sin01(mPhase);
				break;
			case TRI:
				lfo = tri01(mPhase);
				break;
			case RAND:
				lfo = randSmooth(dt);
				break;
		}

		mTarget = lfo * mDepthSec;

		// optional slew limit before smoothing
		if(mSlewSecPerSec > T(0)){
			const T maxStep = mSlewSecPerSec * dt; // seconds per sample
			T d = mTarget - mOut;
			d = scl::clip(d, maxStep, -maxStep);
			mTarget = mOut + d;
		}

		// one-pole smoothing on the output
		mOut += mAlpha * (mTarget - mOut);
		return mOut;
	}

private:
	// params
	Shape mShape = SINE;

	T mRate = T(0.25);         // Hz
	T mDepthSec = T(0);        // seconds (peak)
	T mSmoothSec = T(0.01);    // seconds
	T mSlewSecPerSec = T(0);   // seconds/sec (0 off)

	// random smooth
	T mRandRate = T(0.7);      // Hz-ish update speed
	T mNoiseA = T(0), mNoiseB = T(0);
	T mNoiseT = T(1);          // 0..1 interpolation phase

	// state
	T mPhase = T(0);           // 0..1
	T mOut = T(0);             // smoothed offset (sec)
	T mTarget = T(0);          // target offset (sec)
	T mAlpha = T(0.001);       // smoothing coefficient

	// helpers
	inline void recalcSmooth(){
		// Convert smoothing time constant to 1-pole alpha.
		// If smoothSec == 0 => immediate
		if(mSmoothSec <= T(0)){
			mAlpha = T(1);
			return;
		}
		const T dt = T(Td::ups());
		// standard one-pole: alpha = 1 - exp(-dt/tau)
		mAlpha = T(1) - std::exp(-dt / mSmoothSec);
		mAlpha = scl::clip(mAlpha, T(1), T(0));
	}

	// Sine in [-1,1] from phase in [0,1)
	static inline T sin01(T p){
		// Use std::sin; if you want faster, swap for Gamma poly approx.
		const T twoPi = T(6.28318530717958647692);
		return std::sin(twoPi * p);
	}

	// Triangle in [-1,1] from phase in [0,1)
	static inline T tri01(T p){
		// 0..1 -> -1..1 triangle
		T x = p + T(0.25);
		x -= scl::floor(x);
		// x in [0,1)
		T t = (x < T(0.5)) ? (x * T(4) - T(1)) : (T(3) - x * T(4));
		return t;
	}

	inline T randUni(){
		// lightweight deterministic RNG (xorshift32) local to this unit
		mRng ^= (mRng << 13);
		mRng ^= (mRng >> 17);
		mRng ^= (mRng << 5);
		// map to [-1,1]
		const T u = T(mRng) * (T(1) / T(4294967295.0));
		return u * T(2) - T(1);
	}

	inline T randSmooth(T dt){
		// mNoiseT is interpolation phase 0..1
		const T inc = mRandRate * dt; // cycles/sec * sec/sample = cycles/sample
		mNoiseT += inc;

		if(mNoiseT >= T(1)){
			// advance targets; keep fractional remainder
			mNoiseT -= scl::floor(mNoiseT);
			mNoiseA = mNoiseB;
			mNoiseB = randUni();
		}

		// smoothstep interpolation for continuity of first derivative
		const T t = mNoiseT;
		const T s = t * t * (T(3) - T(2) * t);
		return mNoiseA + (mNoiseB - mNoiseA) * s;
	}

	uint32_t mRng = 0x12345678u;
};

} // namespace gam
