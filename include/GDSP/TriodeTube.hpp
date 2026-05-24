#include <cassert>
#include <cmath>

class TriodeTube : public Function
{
public:
    TriodeTube()
    {
        reset();
        setSampleRate((float)gam::sampleRate());
        // sensible defaults
        setDrive(1.0f);
        setBias(0.0f);
        setCharacter(0.65f);
        setWarmth(0.35f);
        setHysteresis(0.35f);
        setHeadroom(1.0f);
        setGridConduction(0.25f);
    }

    // --------- Must call on SR change ----------
    void setSampleRate(float sr)
    {
        mSR = std::max(8000.0f, sr);

        // bias migration time constants
        // warmth -> slower recovery, slower drift
        updateCoeffs();
    }

    void reset()
    {
        mBiasState   = 0.0f;
        mChargeState = 0.0f;
        mDCZ         = 0.0f;
        mPrevIn      = 0.0f;
    }

    // --------- Musical controls (base knobs) ----------
    void setDrive(float v)          { mDrive = std::max(0.0f, v); }
    void setBias(float v)           { mBias  = std::clamp(v, -1.0f, 1.0f); }     // user bias trim
    void setCharacter(float v)      { mChar  = std::clamp(v, 0.0f, 1.0f); }      // curve shape
    void setWarmth(float v)         { mWarm  = std::clamp(v, 0.0f, 1.0f); updateCoeffs(); } // memory speed
    void setHysteresis(float v)     { mHys   = std::clamp(v, 0.0f, 1.0f); }
    void setHeadroom(float v)       { mHead  = std::max(0.05f, v); }             // overall soft ceiling
    void setGridConduction(float v) { mGrid  = std::clamp(v, 0.0f, 1.0f); }      // asymmetry + “pushback”

    // Optional safety: internal DC reject (you also have BlockDC)
    void setDCReject(float amount01)
    {
        mDCReject = std::clamp(amount01, 0.0f, 1.0f); // 0 off, 1 strong
    }

    // --------- Modulation hooks (optional) ----------
    // These match your ecosystem. Feed any Generator into them if you want.
    // Expect mod signals roughly in [-1,1].
    void setDriveMod(float v)      { driveMod.set(v); }
    void setBiasMod(float v)       { biasMod.set(v); }
    void setCharMod(float v)       { charMod.set(v); }
    void setWarmMod(float v)       { warmMod.set(v); }
    void setHysMod(float v)        { hysMod.set(v); }
    void setHeadMod(float v)       { headMod.set(v); }
    void setGridMod(float v)       { gridMod.set(v); }
    void setAM(float v)            { am.set(v); }
    void setIM(float v)            { im.set(v); }

    float process(float x) override
    {
        // --------- apply input/output gain stages (matches your patterns) ----------
        const float _im = im.process();
        const float _am = am.process();

        x *= _im;

        // --------- per-sample modulated parameters ----------
        float drive = mDrive * (1.0f + driveMod.process());   drive = std::max(0.0f, drive);
        float bias  = mBias  + biasMod.process();             bias  = std::clamp(bias, -1.0f, 1.0f);
        float chr   = mChar  + charMod.process();             chr   = std::clamp(chr, 0.0f, 1.0f);
        float warm  = mWarm  + warmMod.process();             warm  = std::clamp(warm, 0.0f, 1.0f);
        float hys   = mHys   + hysMod.process();              hys   = std::clamp(hys, 0.0f, 1.0f);
        float head  = mHead  * (1.0f + headMod.process());    head  = std::max(0.05f, head);
        float grid  = mGrid  + gridMod.process();             grid  = std::clamp(grid, 0.0f, 1.0f);

        // Warmth changes time constants; update lazily (cheap)
        // If you mod warm at audio-rate, you can remove this and bake warm into coeff mapping below.
        if (std::fabs(warm - mWarmCached) > 1e-4f)
        {
            mWarmCached = warm;
            updateCoeffsFromWarm(warm);
        }

        // --------- build “grid voltage” domain ----------
        float in = x * drive;

        // --------- hysteresis / charge memory (fast-ish) ----------
        // direction-sensitive feel comes from using input derivative
        float dx = in - mPrevIn;
        mPrevIn = in;

        // charge tracks input with lag; hysteresis uses sign of derivative
        mChargeState = mChargeState * mChargeCoeff + in * (1.0f - mChargeCoeff);
        float hysTerm = hys * (0.5f * mChargeState + 0.35f * dx);

        // --------- bias migration (slow) ----------
        // use rectified energy to push bias (tube “bias shift”)
        float e = std::fabs(in);
        // asymmetry: positive drive causes more bias shift when grid conduction is enabled
        float pos = std::max(0.0f, in);
        float driveEnergy = (1.0f - 0.25f * grid) * e + (0.35f * grid) * pos;

        // target bias shift: grows with energy, saturates softly
        float biasTarget = (driveEnergy / (driveEnergy + 1.0f)) * (0.9f * chr + 0.1f);
        // bias migrates toward target then recenters slowly
        mBiasState = mBiasState * mBiasCoeff + biasTarget * (1.0f - mBiasCoeff);
        // recenter / discharge (prevents permanent drift)
        mBiasState *= mBiasRecenter;

        // final effective bias
        float effBias = bias + (mBiasState * (0.85f * chr + 0.15f));

        // --------- grid conduction (soft clamp on positive grid) ----------
        // when grid goes positive, it starts to conduct and “steals” signal -> asymmetric compression
        float vg = in + effBias + hysTerm;
        float vgPos = std::max(0.0f, vg);
        float vgNeg = std::min(0.0f, vg);

        // smooth conduction curve: 0 when negative, rises when positive
        // higher grid => earlier / stronger conduction
        float conduct = grid * softKnee(vgPos, 0.6f, 2.2f);
        vg = vgNeg + (vgPos * (1.0f - 0.35f * conduct));

        // --------- triode “plate current” curve (smooth, asymmetrical) ----------
        // Character maps to curvature: low -> softer, high -> sharper
        float k = 1.2f + 3.5f * chr;         // curvature
        float a = 0.9f + 0.6f * chr;         // asymmetry/odd-even balance
        float y = triodeCurve(vg, k, a);

        // --------- headroom elasticity (soft ceiling that reacts to bias) ----------
        float headEff = head * (1.0f - 0.25f * mBiasState); // bias reduces headroom slightly
        y = softSat(y, headEff);

        // --------- optional internal DC reject ----------
        if (mDCReject > 0.0f)
        {
            // one-pole-ish HP via leaky integrator (cheap)
            // stronger amount -> more DC removal
            float r = 0.995f + (1.0f - mDCReject) * 0.0045f; // ~0.995..0.9995
            float out = y - mDCZ + r * mDCZ;
            mDCZ = y;
            y = out;
        }

        return y * _am;
    }

    // Expose states for metering/debug
    float biasState()   const { return mBiasState; }
    float chargeState() const { return mChargeState; }

    // Modulators (public to match your style)
    Modulator driveMod, biasMod, charMod, warmMod, hysMod, headMod, gridMod;
    Modulator am, im;

private:
    // --- Smooth helper functions (C1 continuous) ---
    static inline float softSat(float x, float head)
    {
        // tanh-based with head scaling; head ~ 0.5..2.0
        float g = 1.0f / std::max(1e-6f, head);
        return head * std::tanh(x * g);
    }

    static inline float softKnee(float x, float knee, float slope)
    {
        // x>=0 expected. Returns ~0 below knee, rises smoothly above.
        // Use log1p-like shape without requiring log for speed.
        float t = std::max(0.0f, x - knee);
        // rational smooth rise
        return (slope * t) / (1.0f + slope * t);
    }

    static inline float triodeCurve(float v, float k, float a)
    {
        // Behavioral triode-ish curve:
        // - asymmetry via a + slight bias-dependent even harmonics
        // - smooth derivative, good for feedback
        //
        // Blend of tanh and “bent” polynomial for richer spectrum.
        float t = std::tanh(v * k);

        // cubic "bend" adds odd harmonics while keeping smoothness
        float b = v - (v*v*v) * 0.18f;

        // asymmetry blend: a in [~0.9..1.5]
        return (0.65f * t + 0.35f * b) * a;
    }

    void updateCoeffs()
    {
        updateCoeffsFromWarm(mWarm);
    }

    void updateCoeffsFromWarm(float warm01)
    {
        // Map warmth to time constants:
        // warm=0 -> faster recovery (cleaner)
        // warm=1 -> slower recovery (thicker memory)
        //
        // Bias: ~30ms..700ms
        // Charge: ~0.5ms..25ms
        float biasMs   = 30.0f  + 670.0f * warm01;
        float chargeMs = 0.5f   + 24.5f  * warm01;

        mBiasCoeff   = std::exp(-1.0f / (0.001f * biasMs   * mSR));
        mChargeCoeff = std::exp(-1.0f / (0.001f * chargeMs * mSR));

        // recenter: very slow bleed to avoid permanent drift
        float recenterMs = 1500.0f + 2500.0f * warm01; // 1.5s..4s
        mBiasRecenter = std::exp(-1.0f / (0.001f * recenterMs * mSR));
    }

private:
    float mSR = 48000.0f;

    // base knobs
    float mDrive = 1.0f;
    float mBias  = 0.0f;
    float mChar  = 0.65f;
    float mWarm  = 0.35f;
    float mHys   = 0.35f;
    float mHead  = 1.0f;
    float mGrid  = 0.25f;

    // optional DC reject
    float mDCReject = 0.0f;

    // state
    float mBiasState   = 0.0f;
    float mChargeState = 0.0f;
    float mDCZ         = 0.0f;
    float mPrevIn      = 0.0f;

    // coeffs
    float mBiasCoeff    = 0.999f;
    float mChargeCoeff  = 0.95f;
    float mBiasRecenter = 0.9999f;

    float mWarmCached = -1.0f;
};
