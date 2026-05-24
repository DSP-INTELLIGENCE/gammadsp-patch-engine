#pragma once
#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

// ------------------------------------------------------------
// StereoDelayRouter (as previously delivered, unchanged)
// ------------------------------------------------------------
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class StereoDelayRouter : public Td {
public:
    StereoDelayRouter(){ reset(); stereo(); }

    void reset(){
        ll = rr = T(1); lr = rl = T(0);
        monoGain = T(1);
        width = T(1);
    }

    void stereo(){ ll = rr = T(1); lr = rl = T(0); }
    void mono(){   ll = rr = T(0.5); lr = rl = T(0.5); }
    void pingPong(){ ll = rr = T(0); lr = rl = T(1); }

    void cross(T amt){
        amt = scl::clip(amt, T(1), T(0));
        ll = rr = T(1) - amt;
        lr = rl = amt;
    }

    void matrix(T LL, T LR, T RL, T RR){
        ll = LL; lr = LR; rl = RL; rr = RR;
    }

    void stereoWidth(T w){ width = scl::clip(w, T(2), T(0)); }
    void monoInputGain(T g){ monoGain = g; }

    inline Vec<2,T> operator()(T x){
        x *= monoGain;
        T l = ll * x + rl * x;
        T r = lr * x + rr * x;
        applyWidth(l, r);
        return Vec<2,T>(l, r);
    }

    inline Vec<2,T> operator()(T inL, T inR){
        T l = ll * inL + rl * inR;
        T r = lr * inL + rr * inR;
        applyWidth(l, r);
        return Vec<2,T>(l, r);
    }

    inline Vec<2,T> operator()(const Vec<2,T>& v){ return (*this)(v[0], v[1]); }

private:
    T ll=T(1), lr=T(0), rl=T(0), rr=T(1);
    T monoGain=T(1);
    T width=T(1);

    inline void applyWidth(T& l, T& r){
        if(width == T(1)) return;
        T mid  = (l + r) * T(0.5);
        T side = (l - r) * T(0.5) * width;
        l = mid + side;
        r = mid - side;
    }
};

// ------------------------------------------------------------
// DelayTimeModulator: smooth + slew-limit a delay time (seconds)
// ------------------------------------------------------------
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class DelayTimeModulator : public Td {
public:
    DelayTimeModulator(){ reset(); }

    void reset(){
        mBase = T(0.25);
        mMod  = T(0);
        mCur  = mBase;
        mSmooth = T(0.002);      // ~2ms-ish smoothing coefficient
        mMaxStep = T(0.001);     // max seconds change per sample (slew limit)
        mMinDelay = T(0.0001);
        mMaxDelay = T(1.0);
    }

    void base(T seconds){ mBase = seconds; }
    void mod (T seconds){ mMod  = seconds; } // additive modulation in seconds

    /// 0..1-ish (internally mapped to a small coefficient)
    void smooth(T s){
        s = scl::clip(s, T(1), T(0));
        // map: 0 -> very slow, 1 -> faster
        mSmooth = T(0.00005) + s * T(0.01);
    }

    /// Slew limit in seconds/sample (keeps modulation “clean”)
    void maxStep(T secondsPerSample){ mMaxStep = scl::max(secondsPerSample, T(0)); }

    void range(T minDelaySec, T maxDelaySec){
        mMinDelay = scl::max(minDelaySec, T(0));
        mMaxDelay = scl::max(maxDelaySec, mMinDelay + T(1e-6));
    }

    /// Set delay directly (base + mod) without smoothing
    void jumpTo(T seconds){
        seconds = scl::clip(seconds, mMaxDelay, mMinDelay);
        mCur = seconds;
        mBase = seconds;
        mMod = T(0);
    }

    /// Returns the smoothed, slew-limited delay time in seconds
    inline T operator()(){
        T tgt = mBase + mMod;
        tgt = scl::clip(tgt, mMaxDelay, mMinDelay);

        // one-pole smoothing
        T next = mCur + mSmooth * (tgt - mCur);

        // slew limit
        T d = next - mCur;
        if(d >  mMaxStep) next = mCur + mMaxStep;
        if(d < -mMaxStep) next = mCur - mMaxStep;

        mCur = next;
        return mCur;
    }

    T value() const { return mCur; }

private:
    T mBase, mMod, mCur;
    T mSmooth;
    T mMaxStep;
    T mMinDelay, mMaxDelay;
};

// ------------------------------------------------------------
// FeedbackProcessor: feedback gain + HP/LP damping + soft sat
// ------------------------------------------------------------
template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class FeedbackProcessor : public Td {
public:
    FeedbackProcessor(){
        reset();
        hp.type(HIGH_PASS);
        lp.type(LOW_PASS);
        setHP(T(20));
        setLP(T(12000));
    }

    void reset(){
        mFbk = T(0.35);
        mDrive = T(0);
        mSatMix = T(0);
        mClip = T(0);
        hp.reset();
        lp.reset();
    }

    void feedback(T g){ mFbk = scl::clip(g, T(0.9995), T(0)); }
    T    feedback() const { return mFbk; }

    void setHP(T hz){ hp.freq(hz); }
    void setLP(T hz){ lp.freq(hz); }

    /// 0..~6 typical; 0 disables tanh stage
    void drive(T d){ mDrive = scl::max(d, T(0)); }

    /// 0..1 blend between linear and tanh stage
    void satMix(T m){ mSatMix = scl::clip(m, T(1), T(0)); }

    /// 0 disables; otherwise simple hard clip threshold (e.g. 1.0)
    void clip(T th){ mClip = scl::max(th, T(0)); }

    inline T operator()(T fbIn){
        // feedback gain first
        T x = fbIn * mFbk;

        // damping filters
        x = hp(x);
        x = lp(x);

        // soft saturation
        if(mDrive > T(0) && mSatMix > T(0)){
            T s = std::tanh(mDrive * x);
            x = (T(1) - mSatMix) * x + mSatMix * s;
        }

        // optional hard clip
        if(mClip > T(0)){
            x = scl::clip(x, mClip, -mClip);
        }

        return x;
    }

    void onDomainChange(double){
        // OnePole in Gamma is domain-aware; if you later swap these to TPT filters,
        // you’d update here.
    }

private:
    T mFbk;
    T mDrive;
    T mSatMix;
    T mClip;

    OnePole<T,T,Td> hp; // using type() as HP/LP
    OnePole<T,T,Td> lp;
};

// ------------------------------------------------------------
// DigitalDelay: full assembly (stereo)
// ------------------------------------------------------------
template<
    class T  = gam::real,
    template<class> class Si = ipl::Switchable, // allows cubic/linear, etc.
    class Td = GAM_DEFAULT_DOMAIN
>
class DigitalDelay : public Td {
public:
    using Stereo = Vec<2,T>;
    using DelayT = Delay<T, Si, Td>;

    DigitalDelay(float maxDelaySec = 2.0f)
    : dL(maxDelaySec), dR(maxDelaySec)
    {
        reset();

        // default interpolation quality
        dL.ipolType(ipl::Type::CUBIC);
        dR.ipolType(ipl::Type::CUBIC);

        // default ranges
        timeL.range(T(0.0001), T(maxDelaySec));
        timeR.range(T(0.0001), T(maxDelaySec));
    }

    void reset(){
        // clear delay buffers if owned
        if(dL.isSoleOwner()) dL.zero();
        if(dR.isSoleOwner()) dR.zero();

        // parameters
        mInGain  = T(1);
        mOutGain = T(1);
        mDry = T(1);
        mWet = T(0.5);

        // base times
        setDelay(T(0.35), T(0.45));
        setFeedback(T(0.35));

        // defaults
        router.stereo();
        router.stereoWidth(T(1));

        timeL.reset();
        timeR.reset();

        fbL.reset();
        fbR.reset();
    }

    // ----------------------------
    // Core controls
    // ----------------------------
    void inGain (T g){ mInGain  = g; }
    void outGain(T g){ mOutGain = g; }

    void dryWet(T wet){
        wet = scl::clip(wet, T(1), T(0));
        mWet = wet;
        mDry = T(1) - wet;
    }

    void dry(T g){ mDry = g; }
    void wet(T g){ mWet = g; }

    void setDelay(T secL, T secR){
        mBaseDL = secL;
        mBaseDR = secR;
        timeL.base(secL);
        timeR.base(secR);
        // set immediately to avoid first-sample jump artifacts
        dL.delay(secL);
        dR.delay(secR);
    }

    void setDelay(T sec){ setDelay(sec, sec); }

    void setFeedback(T g){
        fbL.feedback(g);
        fbR.feedback(g);
    }

    // ----------------------------
    // Quality / interpolation
    // ----------------------------
    void ipol(ipl::Type t){
        dL.ipolType(t);
        dR.ipolType(t);
    }

    // ----------------------------
    // Modulation interface
    // (seconds, additive)
    // ----------------------------
    DelayTimeModulator<T,Td>& modL(){ return timeL; }
    DelayTimeModulator<T,Td>& modR(){ return timeR; }

    void delayMod(T modSecL, T modSecR){
        timeL.mod(modSecL);
        timeR.mod(modSecR);
    }

    // ----------------------------
    // Feedback processing controls
    // ----------------------------
    FeedbackProcessor<T,Td>& feedbackL(){ return fbL; }
    FeedbackProcessor<T,Td>& feedbackR(){ return fbR; }

    // ----------------------------
    // Routing (stereo topology)
    // ----------------------------
    StereoDelayRouter<T,Td>& route(){ return router; }

    void stereo(){ router.stereo(); }
    void pingPong(){ router.pingPong(); }
    void cross(T amt){ router.cross(amt); }
    void width(T w){ router.stereoWidth(w); }

    // ----------------------------
    // Processing
    // ----------------------------
    inline Stereo operator()(T in){
        // mono in, route to stereo input
        Stereo s = router(in);
        return (*this)(s[0], s[1]);
    }

    inline Stereo operator()(T inL, T inR){
        // 1) input gain
        inL *= mInGain;
        inR *= mInGain;

        // 2) read current delay outputs (taps)
        const T tapL = dL.read();
        const T tapR = dR.read();

        // 3) route taps for cross-feedback topology
        //    (router defines how L/R taps feed the feedback processors)
        Stereo fbIn = router(tapL, tapR);

        // 4) feedback processing (gain + damping + saturation)
        const T fbOutL = fbL(fbIn[0]);
        const T fbOutR = fbR(fbIn[1]);

        // 5) write into delays (input + feedback)
        dL.write(inL + fbOutL);
        dR.write(inR + fbOutR);

        // 6) advance delay times (smoothed modulation) and apply
        //    (do after read/write so the time applies “next” sample cleanly)
        const T dtL = timeL();
        const T dtR = timeR();

        dL.delay(scl::clip(dtL, T(dL.maxDelay()), T(0.0001)));
        dR.delay(scl::clip(dtR, T(dR.maxDelay()), T(0.0001)));

        // 7) output mix (use taps = wet signal)
        T outL = inL * mDry + tapL * mWet;
        T outR = inR * mDry + tapR * mWet;

        outL *= mOutGain;
        outR *= mOutGain;

        return Stereo(outL, outR);
    }

    void onDomainChange(double){
        // keep modulators & feedback processors coherent if domain changes
        timeL.base(mBaseDL);
        timeR.base(mBaseDR);
        // filters in FeedbackProcessor are domain-aware; if replaced, update there.
    }

private:
    // Delay lines
    DelayT dL, dR;

    // Time modulation
    DelayTimeModulator<T,Td> timeL, timeR;
    T mBaseDL = T(0.35);
    T mBaseDR = T(0.45);

    // Feedback processing
    FeedbackProcessor<T,Td> fbL, fbR;

    // Routing
    StereoDelayRouter<T,Td> router;

    // Mix / gains
    T mInGain  = T(1);
    T mOutGain = T(1);
    T mDry     = T(1);
    T mWet     = T(0.5);
};

} // namespace gam
