#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "GDSP_Engine.hpp"
#include "GDSP_Parameters.hpp"
#include "VocalTract.hpp"

// -------------------------------------------------------------
// Autonomous Speech Engine
// - Generates evolving "robotic speech" using:
//   * vowel/phoneme sequencing -> ModVocalTract
//   * syllable envelope (mouth open/close)
//   * voiced carrier oscillator + unvoiced consonant noise bursts
//   * pitch drift/jitter + morph automation
// -------------------------------------------------------------
class AutonomousSpeechEngine : public Function
{
public:
    AutonomousSpeechEngine()
    {
        setMix(1.0f);
        setOutputGain(1.0f);

        setTempoBPM(110.0f);      // syllable clock
        setComplexity(0.6f);      // more variation / randomness
        setAutonomy(1.0f);        // 0=input-driven, 1=fully autonomous

        setPitchHz(120.0f);
        setPitchRange(0.35f);     // drift depth
        setJitter(0.10f);         // fast pitch jitter depth

        setRobotTone(0.65f);      // sine <-> saturated
        setVoicedAmount(0.9f);    // voiced excitation strength
        setUnvoicedAmount(0.35f); // consonant noise strength

        setMorphBaseSpeed(0.002f);
        setMorphWobble(0.75f);

        setConsonantRate(0.35f);  // probability per syllable
        setSyllableShape(0.010f, 0.080f); // attack, decay seconds

        setFB(0.15f);             // tract feedback
        setIM(1.0f);
        setAM(1.0f);

        // initialize tract defaults
        tract.setMix(1.0f);
        tract.setFB(0.0f);
        tract.setIM(1.0f);
        tract.setAM(1.0f);
        tract.setMorphSpeed(morphBase);

        // start with a vowel
        setVoice(FormantMorphFilter::Voice::Male);
        pickNewVowel(true);
        pickNewVowel(false);

        reset();
    }

    // ---------- High-level controls ----------
    void setTempoBPM(float bpm)
    {
        bpm = std::clamp(bpm, 30.0f, 240.0f);
        tempoHz = bpm / 60.0f;
    }

    void setComplexity(float v) { complexity = std::clamp(v, 0.0f, 1.0f); }
    void setAutonomy(float v)   { autonomy   = std::clamp(v, 0.0f, 1.0f); } // 0=input-driven, 1=self

    void setPitchHz(float hz)     { basePitch = std::clamp(hz, 40.0f, 500.0f); }
    void setPitchRange(float v)   { pitchRange = std::clamp(v, 0.0f, 1.0f); }
    void setJitter(float v)       { jitter = std::clamp(v, 0.0f, 1.0f); }

    void setRobotTone(float v)    { robotTone = std::clamp(v, 0.0f, 1.0f); }
    void setVoicedAmount(float v) { voicedAmt = std::clamp(v, 0.0f, 1.0f); }
    void setUnvoicedAmount(float v){ unvoicedAmt = std::clamp(v, 0.0f, 1.0f); }

    void setMorphBaseSpeed(float s){ morphBase = std::clamp(s, 0.00001f, 0.1f); }
    void setMorphWobble(float v)   { morphWobble = std::clamp(v, 0.0f, 1.0f); }

    void setConsonantRate(float v) { consonantRate = std::clamp(v, 0.0f, 1.0f); }

    void setSyllableShape(float attackSec, float decaySec)
    {
        attackSec = std::clamp(attackSec, 0.001f, 0.200f);
        decaySec  = std::clamp(decaySec,  0.005f, 0.800f);
        atkSec = attackSec;
        decSec = decaySec;
    }

    void setMix(float v)        { mix = std::clamp(v, 0.f, 1.f); }
    void setOutputGain(float v) { outGain = std::max(0.0f, v); }

    // ---------- ModFilter-style controls ----------
    void setFB(float v) { baseFB = std::clamp(v, -0.99f, 0.99f); }
    void setIM(float v) { im.set(v); }
    void setAM(float v) { am.set(v); }

    // Choose vocal tract "speaker"
    void setVoice(FormantMorphFilter::Voice v) { voice = v; }

    void reset()
    {
        envIn = 0.0f;
        phase = 0.0f;
        clk   = 0.0f;
        mouth = 0.0f;
        mouthVel = 0.0f;

        drift = 0.0f;
        driftVel = 0.0f;

        last = 0.0f;
        noiseState = 0x12345678u;
        consonantEnv = 0.0f;

        tract.setFB(0.0f);
        tract.setIM(1.0f);
        tract.setAM(1.0f);
        tract.setMorphSpeed(morphBase);

        // hard reset is handled inside ModVocalTract/FormantMorphFilter if you have it;
        // otherwise this is fine.
    }

    float process(float input) override
    {
        const float sr = (float)gam::sampleRate();
        const float invSr = 1.0f / std::max(1.0f, sr);

        // -------------------------
        // 1) Input envelope (for articulation coupling when autonomy < 1)
        // -------------------------
        const float inAbs = std::fabs(input);
        // ~20ms follower (simple; you can expose time if desired)
        envIn += (inAbs - envIn) * 0.01f;

        // -------------------------
        // 2) Syllable clock + event scheduling
        // -------------------------
        // Clock rate increases slightly with complexity to sound more "chatty"
        const float talkRateHz = tempoHz * (0.75f + 0.6f * complexity);

        clk += talkRateHz * invSr;
        if (clk >= 1.0f)
        {
            clk -= 1.0f;

            // Step to a new target vowel (probabilistic)
            // Higher complexity = more frequent vowel changes
            if (rand01() < (0.35f + 0.55f * complexity))
                pickNewVowel(false);

            // Occasionally jump current vowel too (stutter/glitch articulation)
            if (rand01() < (0.10f * complexity))
                pickNewVowel(true);

            // Trigger consonant noise burst sometimes
            if (rand01() < consonantRate)
                consonantEnv = 1.0f;

            // Trigger mouth envelope (new syllable)
            triggerMouth();
        }

        // -------------------------
        // 3) Mouth envelope (attack/decay)
        // -------------------------
        stepMouth(sr);

        // Blend autonomy with input-driven articulation
        // autonomy=1 uses internal mouth; autonomy=0 uses input envelope
        const float mouthDrive = lerp(envIn, mouth, autonomy);
        const float mouthGain  = std::clamp(mouthDrive * 1.8f, 0.0f, 1.0f);

        // -------------------------
        // 4) Pitch drift + jitter
        // -------------------------
        // Slow drift (Ornstein-Uhlenbeck-ish)
        driftVel += (randSigned() * 0.0015f - drift * 0.0008f) * (0.5f + complexity);
        drift    += driftVel;
        drift     = std::clamp(drift, -1.0f, 1.0f);

        // Fast jitter (subtle)
        const float jit = randSigned() * jitter;

        float pitch = basePitch * (1.0f + pitchRange * 0.12f * drift) * (1.0f + 0.02f * jit);
        pitch = std::clamp(pitch, 40.0f, 800.0f);

        // -------------------------
        // 5) Voiced carrier (robot excitation)
        // -------------------------
        phase += pitch * invSr;
        if (phase >= 1.0f) phase -= 1.0f;

        float car = std::sin(phase * 6.2831853f);
        // “robot tone” crossfade: sine -> saturated
        const float sat = std::tanh(car * 3.5f);
        car = lerp(car, sat, robotTone);

        // -------------------------
        // 6) Unvoiced consonants (noise bursts)
        // -------------------------
        // fast decay for consonant burst
        consonantEnv += (0.0f - consonantEnv) * 0.02f;
        consonantEnv = std::clamp(consonantEnv, 0.0f, 1.0f);

        float n = randSigned(); // [-1,1]
        // brighten consonant a bit
        float consonant = std::tanh(n * 2.0f) * consonantEnv;

        // -------------------------
        // 7) Combine excitations + optional input injection + feedback
        // -------------------------
        const float voiced   = car * voicedAmt * mouthGain;
        const float unvoiced = consonant * unvoicedAmt * mouthGain;

        // A small amount of input can be injected even when autonomous,
        // to let speech “lock” to the performer if present.
        const float inInject = input * (0.25f * (1.0f - autonomy));

        // Stable feedback (soft-sat)
        float x = (voiced + unvoiced + inInject);
        x = x * im.process() + std::tanh(last) * baseFB;

        // -------------------------
        // 8) Morph automation (vowel motion)
        // -------------------------
        // Morph speed breathes with mouth + complexity + slight randomness
        float morphMod = (0.4f + 0.9f * complexity) * (0.25f + 0.75f * mouthGain);
        morphMod *= (1.0f + morphWobble * 0.35f * randSigned());

        tract.setMorphSpeed(morphBase * (1.0f + morphMod));

        // Drive tract
        float y = tract.process(x);

        // Extra throat saturation (optional but makes it “robotic-real”)
        y = std::tanh(0.85f * y);

        // Output mix (equal-power)
        const float dry = input * std::cos(mix * 1.5707963f);
        const float wet = y     * std::sin(mix * 1.5707963f);

        float out = (dry + wet) * outGain * am.process();
        last = out;
        return out;
    }

private:
    // -------------------------
    // Vowel / phoneme scheduling
    // -------------------------
    void pickNewVowel(bool setCurrent)
    {
        // Replace these with whatever phonemes you have in GDSP_FormantMorphFilter.
        // This table intentionally sticks to classic vowels.
        static constexpr FormantMorphFilter::Phoneme kVowels[] =
        {
            FormantMorphFilter::Phoneme::A,
            FormantMorphFilter::Phoneme::E,
            FormantMorphFilter::Phoneme::I,
            FormantMorphFilter::Phoneme::O,
            FormantMorphFilter::Phoneme::U
        };

        const int count = (int)(sizeof(kVowels) / sizeof(kVowels[0]));
        int idx = (int)(rand01() * (float)count);
        idx = std::clamp(idx, 0, count - 1);

        if (setCurrent)
            tract.setCurrentVowel(voice, kVowels[idx]);
        else
            tract.setTargetVowel(voice, kVowels[idx]);
    }

    // Simple triggered envelope as a damped 2nd-order “mouth” motion (snappy but smooth)
    void triggerMouth()
    {
        // Kick velocity upward; complexity increases “energy”
        mouthVel += 0.8f + 0.9f * complexity;
    }

    void stepMouth(float sr)
    {
        // Convert desired attack/decay into a spring-damper-ish motion
        const float atk = std::max(0.001f, atkSec);
        const float dec = std::max(0.005f, decSec);

        // Approx “stiffness” and “damping” from times
        const float kAtk = 1.0f / (atk * sr);
        const float kDec = 1.0f / (dec * sr);

        // Target depends on whether we’re “opening” (mouthVel kick) or relaxing
        const float target = (mouthVel > 0.01f) ? 1.0f : 0.0f;

        // Pull toward target quickly, relax slowly
        const float k = (target > mouth) ? kAtk : kDec;

        mouth += (target - mouth) * std::clamp(k * 4.0f, 0.0f, 1.0f);

        // Dampen velocity and let it decay
        mouthVel *= 0.92f;
        mouthVel -= 0.02f;
        mouthVel = std::clamp(mouthVel, -2.0f, 2.0f);

        mouth = std::clamp(mouth, 0.0f, 1.0f);
    }

    // -------------------------
    // Deterministic lightweight RNG
    // -------------------------
    inline uint32_t xorshift32()
    {
        uint32_t x = noiseState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        noiseState = x;
        return x;
    }

    inline float rand01()
    {
        // [0,1)
        return (xorshift32() & 0x00FFFFFFu) * (1.0f / 16777216.0f);
    }

    inline float randSigned()
    {
        // [-1,1]
        return rand01() * 2.0f - 1.0f;
    }

    static inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }

private:
    // Core tract
    ModVocalTract tract;
    FormantMorphFilter::Voice voice = FormantMorphFilter::Voice::Male;

    // High-level params
    float tempoHz      = 110.0f / 60.0f;
    float complexity   = 0.6f;
    float autonomy     = 1.0f;

    float basePitch    = 120.0f;
    float pitchRange   = 0.35f;
    float jitter       = 0.10f;

    float robotTone    = 0.65f;
    float voicedAmt    = 0.9f;
    float unvoicedAmt  = 0.35f;

    float morphBase    = 0.002f;
    float morphWobble  = 0.75f;

    float consonantRate= 0.35f;

    float atkSec = 0.010f;
    float decSec = 0.080f;

    float mix     = 1.0f;
    float outGain = 1.0f;

    // Modulators (engine-style)
    Modulator im, am;

    // Feedback + state
    float baseFB = 0.15f;
    float last   = 0.0f;

    // Internal motion state
    float envIn = 0.0f;

    float phase = 0.0f; // carrier
    float clk   = 0.0f; // syllable clock

    float mouth = 0.0f;
    float mouthVel = 0.0f;

    float drift = 0.0f;
    float driftVel = 0.0f;

    float consonantEnv = 0.0f;

    uint32_t noiseState = 0x12345678u;
};
