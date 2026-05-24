#pragma once
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

// If you already have these in your project, include your headers instead.
// (From your earlier messages)
#include "TPT1Pole.h"
#include "TPTSVF.h"

namespace gam {

/// FeedbackProcessor: the “inside-the-loop” block for delays/reverbs
///
/// Typical use:
///     fb = fbProc( delayedSample, inputIntoLoop );
/// then write: delay.write( inputIntoLoop + fb );
///
/// Design goals:
/// - DC safe (HP coupling)
/// - tone shaping (LP damping + optional SVF color)
/// - stable saturation
/// - program-dependent “sag” (slow compression) to tame runaway feedback
///
/// No allocations, Gamma-native operator(), Td-aware.
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class FeedbackProcessor : public Td {
public:
    FeedbackProcessor(){
        reset();
        // sensible defaults for feedback path
        setHPCut(10);
        setDamping(6000);
        setColor(0);          // off by default
        setDrive(T(1));
        setSagAmount(T(0));   // off by default
        setSagSpeedHz(6);     // slow-ish envelope
        setLimit(T(0.98));
    }

    void reset(){
        mHP.reset(T(0));
        mDamp.reset(T(0));
        mColorSVF.reset(T(0), T(0));

        mEnv.reset(T(0));
        mLast = T(0);
    }

    // --- Primary controls ---

    /// DC / rumble removal inside feedback
    void setHPCut(T hz){ mHP.freq(hz); }

    /// “Tape / air loss” damping lowpass inside feedback
    void setDamping(T hz){ mDamp.freq(hz); }

    /// Optional extra color stage (SVF): 0=off, 1=LP, 2=BP, 3=HP, 4=Notch, 5=Peak
    void setColor(int mode){ mColorMode = mode; }

    /// Color SVF params (only used if color mode != 0)
    void setColorFreq(T hz){ mColorSVF.freq(hz); }
    void setColorQ(T q){ mColorSVF.res(q); }

    /// Nonlinear drive amount (pre-sat gain)
    void setDrive(T d){ mDrive = scl::max(d, T(0)); }

    /// Soft limiter threshold before saturation (0..1-ish)
    void setLimit(T lim){ mLimit = scl::clip(lim, T(1), T(0.01)); }

    /// Program-dependent “sag” amount (0..1). Acts like slow compression in feedback.
    void setSagAmount(T a){ mSagAmount = scl::clip(a, T(1), T(0)); }

    /// Sag envelope speed (Hz-ish). Higher = faster response.
    void setSagSpeedHz(T hz){
        // Using 1-pole LP as an envelope smoother; map “hz” to a cutoff.
        // Keep it in a safe range.
        mSagHz = scl::clip(hz, T(60), T(0.05));
        mEnv.freq(mSagHz);
    }

    // --- Convenience “tone” macro knobs (optional) ---

    /// Simple “darker repeats” knob: 0=bright, 1=dark
    void tone(T amt01){
        amt01 = scl::clip(amt01, T(1), T(0));
        // map to a reasonable damping range
        T hz = T(12000) + (T(1200) - T(12000)) * amt01;
        setDamping(hz);
    }

    /// “Character” macro: increases drive + enables mild sag
    void character(T amt01){
        amt01 = scl::clip(amt01, T(1), T(0));
        setDrive(T(1) + amt01 * T(4));
        setSagAmount(amt01 * T(0.35));
    }

    // --- Processing ---

    /// Process a feedback sample.
    /// inLoop is what you’re injecting into the feedback path (typically delayedSample * feedbackGain + inputInject).
    ///
    /// If you don’t want/need inLoop, call operator()(x) instead.
    inline T operator()(T x, T inLoop){
        (void)inLoop; // reserved for future program-dependent behaviors
        return processInternal(x);
    }

    /// Process a feedback sample (single input).
    inline T operator()(T x){
        return processInternal(x);
    }

    /// Last output (for debugging/meters)
    T value() const { return mLast; }

    void onDomainChange(double){
        // Re-apply cached-ish params by calling setters on current values
        // (TPT filters recompute from Td::spu())
        mHP.freq(mHPHzSafe());
        mDamp.freq(mDampHzSafe());
        mEnv.freq(mSagHz);
        // SVF keeps its own params in members; just recalc:
        mColorSVF.onDomainChange(1);
    }

private:
    // --- Internal blocks ---
    TPT1Pole<T, Td> mHP;      // used as highpass via hp()
    TPT1Pole<T, Td> mDamp;    // lowpass damping
    TPTSVF<T, Td>   mColorSVF;

    // Sag envelope follower (smoothed abs)
    TPT1Pole<T, Td> mEnv;

    // Params / state
    int mColorMode = 0;

    T mDrive     = T(1);
    T mLimit     = T(0.98);

    T mSagAmount = T(0);
    T mSagHz     = T(6);

    T mLast      = T(0);

    // Helpers
    inline T mHPHzSafe() const { return T(10); }      // not storing exact user values to keep it lean
    inline T mDampHzSafe() const { return T(6000); }

    inline T softClip(T x) const {
        // Fast, stable, and “musical” in feedback:
        // 1) hard pre-limit
        x = scl::clip(x, mLimit, -mLimit);
        // 2) tanh saturation (keeps it bounded in feedback)
        return std::tanh(x);
    }

    inline T processInternal(T x){
        // --- DC / rumble removal ---
        // (TPT1Pole provides hp() which calls lp() internally)
        x = mHP.hp(x);

        // --- Damping lowpass (classic delay darkening) ---
        x = mDamp.lp(x);

        // --- Optional SVF color stage ---
        if(mColorMode != 0){
            mColorSVF.process(x);
            switch(mColorMode){
                default:
                case 1: x = mColorSVF.low();      break;
                case 2: x = mColorSVF.band();     break;
                case 3: x = mColorSVF.high();     break;
                case 4: x = mColorSVF.notchOut(); break;
                case 5: x = mColorSVF.peakOut();  break;
            }
        }

        // --- Program-dependent sag (slow compression) ---
        if(mSagAmount > T(0)){
            // envelope from current signal magnitude
            T env = mEnv.lp(scl::abs(x));
            // 1 / (1 + k*env)  (bounded, stable)
            T sag = T(1) / (T(1) + mSagAmount * env);
            x *= sag;
        }

        // --- Drive + saturation/limiting ---
        x *= mDrive;
        x = softClip(x);

        mLast = x;
        return x;
    }
};

} // namespace gam
