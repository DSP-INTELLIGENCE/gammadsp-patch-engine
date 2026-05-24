#pragma once
#include "Delay.hpp"

class ModDelay : public Function
{
public:
    ModDelay(float maxDelay=2.0f, float initDelay=0.5f) : _delay(maxDelay,initDelay) {
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.5f);
        setIM(1.0f);        
        setAM(1.0f);
        setDelay(initDelay);
        setFeedback(0.0f);
        setMix(0.5f);
        setIpolType(gam::ipl::Type::CUBIC);
    }
    virtual ~ModDelay() {
        
    }

    
    void setDM(float v) { dm.set(v); }      // delay time mod
    void setFBM(float v) { fbm.set(v); }    // feedback mod
    void setMIXM(float v) { mixm.set(v); }  // mix mod
    void setIM(float v) { im.set(v); }      // input am    
    void setAM(float v) { am.set(v); }      // output am

    void  setDelay(float d) { mBaseDelay = d; _delay.setDelay(d); }
    void  setFeedback(float f) { mBaseFeedback = f; }
    void  setMix(float m) { mBaseMix = m; }
    void  setIpolType(gam::ipl::Type type) { _delay.setIpolType(static_cast<gam::ipl::Type>(type)); }
    
    
    virtual float process(float input) override
    {        
        float delayT   = dm.process(mBaseDelay);        
        float mix      = mixm.process(mBaseMix);

        delayT   = std::clamp(delayT, 0.0001f,_delay.getMaxDelayTime());
        
        mix      = std::clamp(mix, 0.0f, 1.0f);

        _delay.setDelay(delayT);
        
        float d = _delay.read();
        _delay.write(im.process(input) + fbm.process(d * mBaseFeedback));

        return am.process(input * (1.0f - mix) + d * mix);
    }

    float read() const { return _delay.read(); }
    float read(float ago) const { return _delay.read(ago); }
    void  write(const float& v) { _delay.write(v); }

    float getMaxDelayTime() const { return _delay.getMaxDelayTime(); }
    float getBaseDelay() const { return mBaseDelay; }

    Delay& getDelay() { return _delay; }

    Modulator dm, fbm, mixm, im, am;
    float mBaseDelay = 0.5f;
    float mBaseFeedback = 0.0f;
    float mBaseMix = 1.0f;

protected:
    Delay _delay;
};

