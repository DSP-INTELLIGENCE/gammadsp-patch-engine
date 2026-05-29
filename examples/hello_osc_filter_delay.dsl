sr 48000
dur 0.25
node osc Saw 55 0.0
node lp LowPassFilter freq=900 res=1.2
node delay ModDelay 2.0 0.33 feedback=0.35 mix=0.3
graph main osc -> lp -> delay
out main
