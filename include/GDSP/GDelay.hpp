#include "Gamma/Delay.h"

struct GDelay: public gam::Delay<float,gam::ipl::Switchable>, public Function
{
    GDelay(float max, float delayTime): gam::Delay<float,gam::ipl::Switchable>(max,delayTime)
    {
        setDM(0.0f);
        setFBM(0.0f);
        setMIXM(0.5f);
        setIM(1.0f);        
        setAM(1.0f);
        setDelay(delayTime);
        setFeedback(0.0f);
        setMix(0.5f);
        setIpolType(gam::ipl::Type::CUBIC);
    }    
    
    void setDM(float v) { dm.set(v); }      // delay time mod
    void setFBM(float v) { fbm.set(v); }    // feedback mod
    void setMIXM(float v) { mixm.set(v); }  // mix mod
    void setIM(float v) { im.set(v); }      // input am    
    void setAM(float v) { am.set(v); }      // output am

    void  setDelay(float d) { mBaseDelay = d; }
    void  setFeedback(float f) { mBaseFeedback = f; }
    void  setMix(float m) { mBaseMix = m; }
    void  setIpolType(gam::ipl::Type type) { ipolType(static_cast<gam::ipl::Type>(type)); }
    
    float process(float input) override
    {        
        float delayT   = dm.process(mBaseDelay);        
        float mix      = mixm.process(mBaseMix);

        delayT   = std::clamp(delayT, 0.0001f,maxDelay());
        
        mix      = std::clamp(mix, 0.0f, 1.0f);

        delay(delayT);
        
        float d = read();
        write(im.process(input) + fbm.process(d * mBaseFeedback));

        return am.process(input * (1.0f - mix) + d * mix);
    }

    
    float getMaxDelayTime() const { return maxDelay(); }
    float getBaseDelay() const    { return mBaseDelay; }

    Modulator dm, fbm, mixm, im, am;
    float mBaseDelay = 0.5f;
    float mBaseFeedback = 0.0f;
    float mBaseMix = 1.0f;
};

