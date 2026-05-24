// GDSP_Reverb.hpp
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include "GammaDSP.hpp"

enum class ReverbMode { Plate, Hall, Ambient };


// ---- Your Reverb ----
class ReverbModule {
public:
    void prepare(float sr);

    void setSize(float s);       // 0..1
    void setDecay(float d);      // 0..1
    void setPreDelay(float t);   // seconds
    void setDamping(float d);    // 0..1
    void setMix(float m);        // 0..1
    void setMode(ReverbMode m);

    float process(float input);
    void processBlock(float* buffer, size_t n);

    void reset();
    void applySize();
    void setRT60(float rt60Seconds);

private:
    float sampleRate = 44100.f;

    // Core blocks
    Delay preDelay;
    std::array<Delay, 4> early;

    // Diffusion as allpass chain (better than plain delays)
    std::array<AllPass1, 4> diffusers;

    // Tank delays
    std::array<Delay, 2> tankA;
    std::array<Delay, 2> tankB;

    // Feedback state
    float feedbackA = 0.f;
    float feedbackB = 0.f;

    // Tone shaping inside feedback
    OnePole hpA, hpB;    // high-pass (rumble control)
    OnePole dampA, dampB; // low-pass (HF damping)



    // Params
    float size    = 0.5f;
    float decay   = 0.7f;
    float damping = 0.5f;
    float mix     = 0.5f;
    ReverbMode mode = ReverbMode::Hall;

    // Base times (seconds) before scaling by size
    std::array<float, 4> earlyTimes   { 0.013f, 0.021f, 0.034f, 0.055f };
    std::array<float, 4> diffuserTimes{ 0.011f, 0.017f, 0.031f, 0.043f };
    std::array<float, 2> tankTimesA   { 0.149f, 0.211f };
    std::array<float, 2> tankTimesB   { 0.179f, 0.263f };

    void applySize();
    void tuneMode();
    float processTank(float x);

    static float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

    // in ReverbModule private:
    std::array<float,2> tankTimeA_cur {0.149f, 0.211f};
    std::array<float,2> tankTimeB_cur {0.179f, 0.263f};

    float approxLoopSeconds() const {
        float sum = tankTimeA_cur[0] + tankTimeA_cur[1] + tankTimeB_cur[0] + tankTimeB_cur[1];
        return 0.25f * sum;
    }

};

