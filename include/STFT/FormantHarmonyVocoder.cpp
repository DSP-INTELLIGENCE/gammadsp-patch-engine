#include "FormantHarmonyVocoder.hpp"

static inline float semisToRatio(float semis)
{
    return std::pow(2.0f, semis / 12.0f);
}

// Very small, deterministic noise (no malloc, no rand()).
// Xorshift32-ish in float form. Good enough for breath/consonant texture.
float FormantHarmonyVocoder::noise01()
{
    static unsigned s = 0x1234567u;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    // [-1, 1]
    return ((s & 0xFFFFFFu) / float(0x7FFFFFu)) * 2.0f - 1.0f;
}

static float hzToQ(float f)
{
    return std::max(0.6f, f * 0.0012f);
}

FormantHarmonyVocoder::FormantHarmonyVocoder(float sr, int bands)
: mSampleRate(sr), mBands(std::max(4, bands))
{
    mBand.resize(mBands);
    mEnvCache.resize(mBands, 0.0f);

    // Default harmony: unison
    mHarmony[0] = 0;
    mHarmonyCount = 1;

    // Log-spaced band centers
    for (int i = 0; i < mBands; ++i)
    {
        float t = (float)i / (float)(mBands - 1);
        float freq = mFmin * std::pow(mFmax / mFmin, t);
        float Q = hzToQ(freq);

        mBand[i].analysis.setBandpass(freq, Q, sr);
        mBand[i].synth.setBandpass(freq, Q, sr);
    }

    recomputeShift();
    reset();
}

void FormantHarmonyVocoder::reset()
{
    for (auto& b : mBand)
    {
        b.analysis.reset();
        b.synth.reset();
        b.env.value = 0.0f;
    }
    std::fill(mEnvCache.begin(), mEnvCache.end(), 0.0f);

    for (int i = 0; i < 8; ++i) mPhase[i] = 0.0f;
    mVibPhase = 0.0f;
}

void FormantHarmonyVocoder::setHarmony(const int* semis, int count)
{
    mHarmonyCount = std::clamp(count, 1, 8);
    for (int i = 0; i < mHarmonyCount; ++i)
        mHarmony[i] = semis[i];
}

void FormantHarmonyVocoder::recomputeShift()
{
    // Convert semitone formant shift into band offset in log space.
    // One octave = 12 semis corresponds to log2 ratio = 1.
    // Bands are log-spaced between Fmin..Fmax.
    float ratio = semisToRatio(mFormantShiftSemis);

    // Map ratio onto band index scale:
    // bands cover log(Fmax/Fmin). ShiftBands = log(ratio) / log(Fmax/Fmin) * (bands-1)
    float logSpan = std::log(mFmax / mFmin);
    float logR    = std::log(std::max(ratio, 0.0001f));
    mShiftBands = (logSpan > 0.0f) ? (logR / logSpan) * (float)(mBands - 1) : 0.0f;
}

float FormantHarmonyVocoder::getShiftedEnv(int i) const
{
    // We want to move the envelope shape across bands.
    // If shifting formants UP (positive semis), apply lower-band env to higher-band synth bands.
    // That means envIndex = i - shiftBands.
    float idx = (float)i - mShiftBands;

    // Linear interpolation between cached envs
    int i0 = (int)std::floor(idx);
    int i1 = i0 + 1;
    float f = idx - (float)i0;

    i0 = std::clamp(i0, 0, mBands - 1);
    i1 = std::clamp(i1, 0, mBands - 1);

    float e0 = mEnvCache[i0];
    float e1 = mEnvCache[i1];
    return e0 + (e1 - e0) * f;
}

float FormantHarmonyVocoder::makeInternalCarrier(float pitchHz, float intensity, float brightness)
{
    // If no pitch, return noise-only carrier
    if (pitchHz <= 0.0f)
        return noise01() * 0.2f;

    // Vibrato in semitones
    float vibRatio = 1.0f;
    if (mVibDepthSemis > 0.0f && mVibRate > 0.0f)
    {
        mVibPhase += mVibRate / mSampleRate;
        if (mVibPhase >= 1.0f) mVibPhase -= 1.0f;

        float vib = std::sin(6.2831853f * mVibPhase);
        vibRatio = semisToRatio(vib * mVibDepthSemis);
    }

    float out = 0.0f;

    // brightness shapes waveform: closer to saw at high brightness, closer to sine at low
    float sawAmt  = std::clamp(brightness, 0.0f, 1.0f);
    float sineAmt = 1.0f - sawAmt;

    // Intensity scales the carrier amplitude a bit
    float amp = 0.2f + 0.6f * std::clamp(intensity, 0.0f, 1.0f);

    for (int k = 0; k < mHarmonyCount; ++k)
    {
        float f = pitchHz * semisToRatio((float)mHarmony[k]) * vibRatio;
        f = std::clamp(f, 20.0f, 12000.0f);

        mPhase[k] += f / mSampleRate;
        if (mPhase[k] >= 1.0f) mPhase[k] -= 1.0f;

        // Saw [-1,1]
        float saw = 2.0f * mPhase[k] - 1.0f;
        // Sine [-1,1]
        float s   = std::sin(6.2831853f * mPhase[k]);

        out += (saw * sawAmt + s * sineAmt);
    }

    out /= (float)mHarmonyCount;

    // Add a bit of noise for consonants/breath
    out = out * amp + noise01() * (mNoiseMix * 0.35f);

    return out;
}

float FormantHarmonyVocoder::process(float modulator,
                                     float carrierIn,
                                     const AudioFeatures& F,
                                     const MusicalState&  M,
                                     const ExpressionState& E)
{
    (void)M;

    // --- 1) Analysis pass: compute & cache per-band envelopes ---
    for (int i = 0; i < mBands; ++i)
    {
        float m = mBand[i].analysis.process(modulator);
        float drive = std::abs(m);

        // Musical driving (so envelopes “speak” with your engine)
        drive *= (0.45f + 0.65f * F.energy);
        if (F.onset) drive *= 1.15f;
        drive *= (0.60f + 0.70f * E.intensity);

        // Optional: noise can increase consonant articulation
        drive *= (0.85f + 0.30f * F.noise);

        mEnvCache[i] = mBand[i].env.process(drive, mSampleRate);
    }

    // --- 2) Carrier: external + internal (pitch-locked harmony) ---
    float internal = makeInternalCarrier(F.pitch, E.intensity, E.brightness);
    float carrier  = carrierIn * (1.0f - mInternalCarrierMix) + internal * mInternalCarrierMix;

    // --- 3) Synthesis pass: apply (shifted) envelopes to synth bands ---
    float out = 0.0f;

    for (int i = 0; i < mBands; ++i)
    {
        float env = getShiftedEnv(i);

        // Optional emphasis for intelligibility / energy
        env *= (0.7f + 0.6f * E.intensity);

        float c = mBand[i].synth.process(carrier);
        out += c * env;
    }

    // Overall gain trim
    out *= 0.6f;

    // Dry/Wet: wet is vocoded output, dry is (optionally) carrier
    return out * mDryWet + carrierIn * (1.0f - mDryWet);
}
