#pragma once
#include "Comb.hpp"
#include "Parameters.hpp"

class ModComb : public Function
{
public:
    ModComb(float maxDelay = 2.0f, float initDelay=0.05f) 
    : _comb(maxDelay,initDelay)
    {
        setDM(initDelay);
        setFBM(0.0f);
        setAPM(0.0f);
        setMIXM(0.0f);
        setAM(1.0f);
        setIM(1.0f);
        setDelay(initDelay);
        setMix(0.5f);
        setFeedback(0.0f);
        setFeedforward(0.0f);
        setAllPass(0.0f);
        setIpolType(gam::ipl::Type::CUBIC);

    }
    virtual ~ModComb() = default;

    // ---------------- Modulation Controls ----------------
    void setDM(float v) { dm.set(v); }  // feedback modulation
    void setFBM(float v) { fm.set(v); }  // feedback modulation
    void setFFM(float v) { ff.set(v); }  // feedforward modulation
    void setAPM(float v) { ap.set(v); }  // allpass modulation
    void setMIXM(float v) { mm.set(v); }  // mix modulation
    void setAM(float v) { am.set(v); }  // amplitude modulation
    void setIM(float v) { im.set(v); }  // amplitude modulation    

    // ---------------- Base Controls ----------------
    void  setDelay(float d)     { _comb.setDelay(d); mDelayTime = d; }
    void  setMix(float m)       { mMix = m; }
    void  setFeedback(float v)  { _comb.setFeedback(v); mFeedback = v; }
    void  setFeedforward(float v){ _comb.setFeedforward(v); mFeedForward = v; }
    void  setAllPass(float v)   { _comb.setAllPass(v); mAllPass = v; }
    void  setIpolType(gam::ipl::Type type) { _comb.setIpolType(static_cast<gam::ipl::Type>(type)); }

    float getDelayTime()        { return _comb.getDelayTime(); }
    float getMaxDelayTime() const      { return _comb.getMaxDelayTime(); }

    // ---------------- Processing ----------------
    virtual float process(float input) override
    {
        return update(input);
    }

    float update(float input)
    {        
        float dt       = dm.process(mDelayTime);
        float feedback = fm.process(mFeedback);
        float ffwd     = ff.process(mFeedForward);
        float allpass  = ap.process(mAllPass);
        float mix      = mm.process(mMix);
        
        // Safety
        dt       = std::clamp(dt,0.0001f, getMaxDelayTime());
        feedback = std::clamp(feedback, -0.999f, 0.999f);
        ffwd     = std::clamp(ffwd, -0.999f, 0.999f);
        allpass  = std::clamp(allpass, -0.999f, 0.999f);
        mix      = std::clamp(mix, 0.0f, 1.0f);

        // Apply modulation
        _comb.setDelay(dt);
        _comb.setFeedback(feedback);
        _comb.setFeedforward(ffwd);
        _comb.setAllPass(allpass);

        float y = _comb.process(im.process(input));
        
        // Dry / Wet
        float out = input * (1.0f - mix) + y * mix;

        // Final amplitude modulation
        return am.process(out);
    }

    // ---------------- Modulators ----------------
    Modulator dm, fm, ff, ap, mm, am, im;
    float mDelayTime;
    float mFeedback;
    float mFeedForward;
    float mAllPass;
    float mMix;

protected:
    Comb _comb;
};
