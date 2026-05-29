sr 48000
dur 0.25
node lp LowPassFilter freq=1200 res=1.0
node delay ModDelay 2.0 0.2 feedback=0.25 mix=0.25
node clip SoftClip
graph fx input -> lp -> delay -> clip
out fx
