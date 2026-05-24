#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DattorroVerb : public Td {
public:
	using Stereo = Vec<2,T>;

	DattorroVerb(){
		// ---- Input diffusion ----
		in1.delay(0.0048f); in1.gain(0.75f);
		in2.delay(0.0035f); in2.gain(0.75f);
		in3.delay(0.0027f); in3.gain(0.75f);
		in4.delay(0.0018f); in4.gain(0.75f);

		// ---- Tank diffusion ----
		apL1.delay(0.022f); apL1.gain(0.7f);
		apL2.delay(0.030f); apL2.gain(0.7f);
		apR1.delay(0.033f); apR1.gain(0.7f);
		apR2.delay(0.037f); apR2.gain(0.7f);

		// ---- Long delays (tank) ----
		dL1.delay(0.060f);
		dL2.delay(0.089f);
		dR1.delay(0.073f);
		dR2.delay(0.097f);

		// Gamma-native “Modulator” stand-ins:
		// - decayL/decayR: damping in feedback (lowpass)
		// - fbmL/fbmR: smoothing on injection into tank
		decayL.type(LOW_PASS); decayR.type(LOW_PASS);
		fbmL.type(LOW_PASS);   fbmR.type(LOW_PASS);

		decayL.freq(2000); decayR.freq(2000);
		fbmL.freq(2000);   fbmR.freq(2000);
	}

	void setDecay(float d){ mDecay = scl::clip(d, 0.99f, 0.2f); }

	// Optional: expose damping / smoothing if you want control parity with your Modulator
	void setDamping(float hz){
		decayL.freq(hz); decayR.freq(hz);
	}
	void setFbmSmooth(float hz){
		fbmL.freq(hz); fbmR.freq(hz);
	}

	Vec<2,T> operator()(T x) {
		// ---- Input diffusion (exact topology) ----
		T y = in4( in3( in2( in1(x) ) ) );

		// ---- Tank feedback taps (exact topology) ----
		T tapR = (dR2.read() + dR1.read()) * T(0.5);
		T tapL = (dL2.read() + dL1.read()) * T(0.5);

		T fbL = decayL(tapR * T(mDecay));
		T fbR = decayR(tapL * T(mDecay));

		// ---- Left tank (exact order) ----
		T l = apL2( dL2( apL1( dL1( fbmL(y + fbL) ) ) ) );

		// ---- Right tank (exact order) ----
		T r = apR2( dR2( apR1( dR1( fbmR(y + fbR) ) ) ) );

        return Vec<2,T>(l,r);
    }
	float mDecay = 0.85f;

private:
	// Gamma-native building block: delay-based allpass (matches “Comb with setDelay + setAllPass”)
	struct APDelay {
		Delay<T, ipl::Linear> d;
		T g = T(0.75);

		APDelay(float maxDelaySec=0.2f){
			d.maxDelay(maxDelaySec);
			d.delay(0);
		}

		void delay(float sec){ d.delay(sec); }
		void gain(float v){ g = T(v); }

		// Standard 1st-order allpass using explicit delay line:
		// y = -g*x + x_d + g*y_d, implemented without needing extra delay for y
		T operator()(T x){
			T xd = d.read();
			T y  = xd - g*x;
			d.write(x + g*y);
			return y;
		}
	};

	// Gamma-native long delay wrapper
	struct LongDelay {
		Delay<T, ipl::Linear> d;
		LongDelay(float maxDelaySec=0.2f){
			d.maxDelay(maxDelaySec);
			d.delay(0);
		}
		void delay(float sec){ d.delay(sec); }
		T read() const { return d.read(); }
		T operator()(T x){ T y = d.read(); d.write(x); return y; }
	};

	// Input diffusion
	APDelay in1{0.05f}, in2{0.05f}, in3{0.05f}, in4{0.05f};

	// Tank diffusion
	APDelay apL1{0.05f}, apL2{0.05f}, apR1{0.05f}, apR2{0.05f};

	// Long delays (tank)
	LongDelay dL1{0.2f}, dL2{0.2f}, dR1{0.2f}, dR2{0.2f};

	// “Modulator” replacements (Gamma-native)
	OnePole<T,T,Td> decayL, decayR;
	OnePole<T,T,Td> fbmL, fbmR;
};

} // namespace gam
