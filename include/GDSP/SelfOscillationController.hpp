struct SelfOscillationController
{
    // Controls
    float engage    = 1.f;    // how easily it enters oscillation
    float targetAmp = 0.5f;  // desired oscillation amplitude
    float stiffness = 0.002f;// how rigid the control is

    // State
    float oscEnv = 0.f;

    inline float zap(float x){
        return (fabsf(x) < 1e-20f) ? 0.f : x;
    }

    inline float process(float fb, float inputEnergy)
    {
        // detect oscillation energy
        float e = fabsf(fb);

        // envelope follower
        oscEnv += 0.01f * (e - oscEnv);

        // how close we are to oscillation
        float drive = engage * oscEnv;

        // regulate oscillation amplitude
        float error = targetAmp - oscEnv;
        float control = stiffness * error;

        // apply soft correction
        fb += control * fb;

        // allow input to influence oscillation
        fb += 0.1f * inputEnergy * fb;

        return zap(fb);
    }
};
