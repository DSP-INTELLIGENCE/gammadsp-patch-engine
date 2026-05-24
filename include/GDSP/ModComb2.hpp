#pragma once
#include "Comb.hpp"
#include "Parameters.hpp"

class ModComb2 : public Function
{
public:
    ModComb2(float maxDelay = 2.0f, float initDelay = 0.05f)
    : _comb(maxDelay, initDelay)
    {
        baseDelay     = initDelay;
        baseFeedback  = 0.0f;
        baseFF        = 0.0f;
        baseAP        = 0.0f;
        baseMix       = 0.5f;

        setAM(1.0f);
        setIM(1.0f);
    }

    // ---------------- Base controls ----------------
    void setDelay(float d)        { baseDelay = d; }
    void setFeedback(float v)     { baseFeedback = v; }
    void setFeedforward(float v)  { baseFF = v; }
    void setAllPass(float v)      { baseAP = v; }
    void setMix(float v)          { baseMix = v; }

    // ---------------- Modulation depths ----------------
    void setDM(float v)   { dm.set(v); }
    void setFBM(float v)  { fm.set(v); }
    void setFFM(float v)  { ff.set(v); }
    void setAPM(float v)  { ap.set(v); }
    void setMIXM(float v) { mm.set(v); }

    // ---------------- Gain stages ----------------
    void setAM(float v) { am.set(v); }
    void setIM(float v) { im.set(v); }

    float process(float input) override
    {
        float _dm  = dm.process(baseDelay);
        float _fb  = fm.process(baseFeedback);
        float _ff  = ff.process(baseFF);
        float _ap  = ap.process(baseAP);
        float _mix = mm.process(baseMix);

        
        float delay    = std::clamp(_dm, 0.0001f, _comb.getMaxDelayTime());
        float feedback = std::clamp(_fb, -0.999f, 0.999f);
        float ffwd     = std::clamp(_ff, -0.999f, 0.999f);
        float allpass  = std::clamp(_ap, -0.999f, 0.999f);
        float mix      = std::clamp(_mix, 0.0f, 1.0f);

        _comb.setDelay(delay);
        _comb.setFeedback(feedback);
        _comb.setFeedforward(ffwd);
        _comb.setAllPass(allpass);

        float y = _comb.process(im.process(input));

        float out = input * (1.0f - mix) + y * mix;

        return am.process(out);
    }

private:
    Comb _comb;

    float baseDelay;
    float baseFeedback;
    float baseFF;
    float baseAP;
    float baseMix;

    Modulator dm, fm, ff, ap, mm;
    Modulator am, im;
};
