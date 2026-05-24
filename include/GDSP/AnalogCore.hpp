// AnalogCore.hpp

struct Triode: public Function
{
    T mu  = T(100);
    T kg1 = T(1060);
    T kp  = T(600);
    T ex  = T(1.4);
    T vct = T(0.0);

    inline T process(T vgk, T vpk)
    {
        T e = vgk + vct + vpk / mu;

        // Smooth cutoff
        e = std::max(e, T(0));

        // Soft saturation of drive domain
        e = std::tanh(T(0.7) * e);

        // Plate resistance interaction
        T denom = T(1) + kp * std::max(T(0.05), vpk);

        T ip = kg1 * std::pow(e, ex) / denom;

        // Physical clamp: current never negative
        return std::clamp(ip, T(0), T(10));
    }
};



struct TriodeGainStage : public Function
{
    AnalogGainCell<T> cell;
    Triode<T> triode;
    TPT1Pole<T> plateLP;

    void init(T sr)
    {
        cell.init(sr);
        plateLP.setLPCut(T(8000), sr);
        reset();
    }

    void reset()
    {
        cell.reset();
        plateLP.reset();
    }

    T process(T x) override
    {
        T vgk = cell.process(x);
        T vpk = std::max(T(0.1), plateLP.s);

        T ip = triode.process(vgk, vpk);
        return plateLP.processLP(-ip);
    }
};


struct TriodeStage: public Function
{
    T sr = T(44100);

    // --- Core tube device ---
    Triode<T> triode;

    // --- Analog conditioning blocks ---
    TPT1Pole<T> inputHP;     // input coupling capacitor
    TPT1Pole<T> biasLP;      // slow bias memory
    TPT1Pole<T> plateLP;     // plate RC load (voltage state)

    // --- State ---
    T bias = T(0);

    // --- User controls ---
    T drive      = T(2.5);   // input gain
    T biasAmount = T(0.4);   // envelope → bias depth

    // --- Setup ---
    void init(T fs)
    {
        sr = fs;

        inputHP.setCut(T(20), sr);     // typical coupling capacitor
        biasLP.setCut(T(5), sr);       // very slow bias memory
        plateLP.setCut(T(8000), sr);   // plate load bandwidth

        reset();
    }

    void reset()
    {
        inputHP.reset();
        biasLP.reset();
        plateLP.reset();
        bias = T(0);
    }

    inline T process(T x)
    {
        // Input conditioning
        x = inputHP.processHP(x);

        // Envelope → slow bias migration
        T env = std::fabs(x);
        T targetBias = -biasAmount * env;
        bias = biasLP.processLP(targetBias);

        // Grid voltage
        T vgk = x * drive + bias;

        // Continuous grid conduction softening
        T gpos = std::max(T(0), vgk);
        vgk -= T(0.5) * gpos;

        // Plate voltage from previous state
        T vpk = std::max(T(0.1), plateLP.s);

        // Triode current
        T ip = triode.process(vgk, vpk);
        ip = std::clamp(ip, -T(10), T(10));   // numeric safety

        // Plate RC load integration
        T y = plateLP.processLP(-ip);

        // Keep plate voltage physically valid
        plateLP.s = std::max(T(0.1), plateLP.s);

        return y;
    }

};

struct FinalOutputStage: public Function
{

    // Soft clip amount (0..1)
    T clipAmount = T(0.5);
    T clipLP = T(0.5);

    // Dither amount (in LSBs)
    T ditherAmount = T(1.0); // 1 LSB by default

    // State
    std::mt19937 rng { 0x12345678 };
    std::uniform_real_distribution<T> uni { -T(1), T(1) };

    
    inline T softClip(T x)
    {
        return x / (T(1) + std::fabs(x));
    }

    // Triangular PDF dither
    inline T tpdf()
    {
        return (uni(rng) + uni(rng)) * T(0.5);
    }

    T process(T x)
    {
        // smooth clip control
        clipLP += T(0.002) * (clipAmount - clipLP);

        // soft clip
        x = softClip(x * (T(1) + clipLP * T(4)));

        // TPDF dither (assuming T->16/24bit later)
        T dither = tpdf() * ditherAmount * (T(1) / T(32768));
        x += dither;

        return x;
    }
};



struct HardClipper: public Function
{
    // Controls
    T threshold = T(1);   // clipping point
    T drive     = T(1);   // how hard we hit it
    T asym      = T(0);   // even harmonic bias
    T trim      = T(1);

    // Smoothing
    T thrLP  = T(1);
    T drvLP  = T(1);
    T asymLP = T(0);


    // Core clip with soft knee transition
    inline T clipCore(T x)
    {
        T t = thrLP;

        // soft knee around threshold
        T y;
        if (x > t) {
            T d = x - t;
            y = t + d / (T(1) + d * d);
        } else if (x < -t) {
            T d = x + t;
            y = -t + d / (T(1) + d * d);
        } else {
            y = x;
        }

        // asymmetry
        y += asymLP * y * y;

        return y;
    }

    // Loudness compensation
    inline T loudComp() const {
        return T(1) / (T(1) + T(0.3) * drvLP);
    }

    inline T process(T x)
    {
        // smooth controls
        thrLP  += T(0.002) * (threshold - thrLP);
        drvLP  += T(0.002) * (drive     - drvLP);
        asymLP += T(0.002) * (asym      - asymLP);

        // drive in
        T y = x * drvLP;

        // clip
        y = clipCore(y);

        // output trim & loudness
        y *= trim * loudComp();

        return y;
    }
};



struct HarmonicEnhancer : public Function
{
    // user control 0..1
    T amount = T(0.3);
    T amountLP = T(0.3);

    // optional emphasis on high-frequency transients
    TPT1Pole<T> highHP;
    TPT1Pole<T> highLP;

    T sr = T(44100);

    void prepare(T sampleRate)
    {
        sr = sampleRate;
        highHP.setHPCut(T(4000), sr);
        highLP.setLPCut(T(16000), sr);
    }

    void reset()
    {
        highHP.reset();
        highLP.reset();
        amountLP = amount;
    }
    
    inline T sat(T x)
    {
        return x / (T(1) + std::fabs(x));
    }

    T process(T x) override
    {
        // smooth amount
        amountLP += T(0.002) * (amount - amountLP);

        // isolate upper band for harmonic generation
        T high = highLP.processLP(highHP.processHP(x));

        // generate subtle harmonics
        T h = sat(high * T(2)) - high;

        // blend harmonics back in
        x += h * (amountLP * T(0.25));

        return x;
    }
};




// InputGain: transparent excitation control.
// - Smooths in dB domain via your ParamLinear
// - Converts to linear gain per-sample
// - One shared gain for stereo safety

class InputGain : public Function
{
public:
    InputGain(T initDb = T(0.0), T smoothMs = T(10.0))
    {
        // Attach your smoother to the parameter
        mSmooth.setTime(smoothMs);
        mGainDb.attachSmoother(&mSmooth);
        reset(initDb);
    }

    // --- Controls ---
    void setGainDb(T db)               { mGainDb.set(db); }
    T gainDb() const                   { return mGainDb._value; } // raw param value
    void setSmoothingMs(T ms)          { mSmooth.setTime(std::max(T(0.0), ms)); }
    bool isRamping() const                 { return mSmooth.isRamping(); }

    void reset(T db = T(0.0))
    {
        // Hard set + reset ramp state
        mGainDb._value = db;
        mSmooth = ParamLinear(db, mSmoothMs());  // re-init ramp at current db
        mGainDb.attachSmoother(&mSmooth);
        mLastLin = dbToLin(db);
    }

    // --- DSP (mono) ---
    T process(T x) override
    {
        // Smooth dB -> linear
        T db  = mGainDb.process();
        T lin = dbToLin(db);

        // Denormal guard for very small gains
        lin += T(1e24);

        mLastLin = lin;
        return x * lin;
    }

    // --- Stereo helper (recommended) ---
    inline StereoSample processStereo(T inL, T inR)
    {
        T db  = mGainDb.process();
        T lin = dbToLin(db);
        lin += T(1e24);

        mLastLin = lin;
        return StereoSample(inL * lin, inR * lin);
    }

    // --- Buffer helpers ---
    void run(const T* in, T* out, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

    void runStereo(const T* inL, const T* inR,
                   T* outL, T* outR, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            StereoSample s = processStereo(inL[i], inR[i]);
            outL[i] = s.outL;
            outR[i] = s.outR;
        }
    }

    // For metering / downstream use
    T gainLinear() const { return mLastLin; }

private:
    static inline T dbToLin(T db)
    {
        // 10^(dB/20)
        return std::pow(T(10.0), db * (T(1.0) / T(20.0)));
    }

    T mSmoothMs() const { return mSmooth.value(); } // not critical; just keeps reset tidy

private:
    Parameter   mGainDb;
    ParamLinear mSmooth { T(0.0), T(10.0) };
    T       mLastLin = T(1.0);
};

struct InputNormalizer
{
    // target RMS level
    T target = T(0.25);

    // smoothing
    T attack  = T(0.01);
    T release = T(0.002);

    // state
    T env = T(0);
    T gain = T(1);

    
    T process(T x)
    {
        T a = fabsf(x);
        env += (a - env) * ((a > env) ? attack : release);

        T desired = target / (env + T(1e9));

        // smooth gain
        gain += T(0.001) * (desired - gain);

        return x * gain;
    }
};



struct LoudnessCompensator : public Function
{
    T target = T(0.25);   // target RMS
    T rms = T(0);
    T gain = T(1);


    T process(T x) override
    {
        rms += (x * x - rms) * T(0.001);
        T cur = std::sqrt(rms) + T(1e12);

        T targetGain = target / cur;
        gain += (targetGain - gain) * T(0.001);

        return x * gain;
    }
};

struct ToneStack
{
    virtual ~ToneStack() = default;

    virtual void prepare(T sr) = 0;
    virtual void reset() = 0;

    // tone01 = 0..1
    virtual T process(T x, T tone01) = 0;
};

struct MarshallToneStack : public ToneStack
{
    // --- Filters ---
    Biquad bass;     // low shelf
    Biquad mid;      // peaking
    Biquad treble;   // high shelf

    // --- Internal gain normalization ---
    T makeup = T(1.0);

    void prepare(T sr) override
    {
        // Bass shelf ~100 Hz
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(T(100));
        bass.setRes(T(0.707));
        bass.setLevel(T(0));  // dB, controlled dynamically

        // Mid band ~650 Hz (Marshall signature)
        mid.setType(gam::PEAKING);
        mid.setFreq(T(650));
        mid.setRes(T(0.7));
        mid.setLevel(T(0));

        // Treble shelf ~4 kHz
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(T(4000));
        treble.setRes(T(0.707));
        treble.setLevel(T(0));

        // Output trim (prevents level jump)
        makeup = T(0.85);
    }

    void reset() override
    {
        bass.reset();
        mid.reset();
        treble.reset();
    }

    // bass01, mid01, treble01 ∈ [0,1]
    T process(T x, T bass01, T mid01, T treble01)
    {
        // ----- Control law (Marshall-like) -----
        // Bass: ±10 dB
        T bassDB = -T(10) + T(20) * bass01;

        // Mid: deep scoop to boost (+8 dB to -12 dB)
        T midDB  = -T(12) + T(20) * mid01;

        // Treble: ±12 dB
        T trebDB = -T(12) + T(24) * treble01;

        bass.setLevel(bassDB);
        mid.setLevel(midDB);
        treble.setLevel(trebDB);

        // ----- Filter order matters -----
        T y = x;
        y = bass.process(y);     // low end first
        y = mid.process(y);      // mid shaping
        y = treble.process(y);   // sparkle last

        return y * makeup;
    }
};

// -----------------------------------------------------------------------------
// MagneticTransformerStage (drop-in like TriodeTube)
// - Stateful "iron" output transformer / power interaction stage
// - LF saturates first (flux / core model), HF stays clearer
// - Hysteresis + recovery ("iron") + dynamic headroom ("load")
// - Smooth, feedback-safe nonlinearity (tanh-based), no hard corners
//
// Controls (musical):
//   Drive : how hard you hit the transformer
//   Size  : core size / saturation threshold (bigger = more headroom)
//   Iron  : hysteresis + recovery time (more = thicker, slower memory)
//   Load  : dynamic headroom / compression feel (more = softer, heavier)
//   Mix   : dry/wet blend
//
// Modulation:
// - im/am are audio modulators (same convention as your FrequencyShifter)
// - driveMod/sizeMod/ironMod/loadMod/mixMod are scalar modulators (no-arg process)
//
// Notes:
// - Intended to sit after TriodeTube or inside feedback paths.
// - Flux/hysteresis update occurs as part of process() but remains smooth.
// -----------------------------------------------------------------------------
class MagneticTransformerStage : public Function
{
public:
    MagneticTransformerStage()
    {
        reset();
        setSampleRate((T)gam::sampleRate());

        setDrive(T(1.0));
        setSize(T(0.6));
        setIron(T(0.5));
        setLoad(T(0.5));
        setMix(T(1.0));

        setLFCornerHz(T(180.0));    // transformer "core" is mostly LF-driven
        setPhaseSmear(T(0.25));     // subtle time smear
        setDCReject(T(0.0));        // optional (you can also place BlockDC after)
    }

    void setSampleRate(T sr)
    {
        mSR = std::max(T(8000.0), sr);
        updateSplit();
        updateMemoryCoeffs();
        updatePhase();
    }

    void reset()
    {
        mFlux = T(0.0);
        mHys  = T(0.0);
        mEnv  = T(0.0);

        mLP   = T(0.0);

        mAPz1 = T(0.0);
        mAPz2 = T(0.0);

        mDCZ  = T(0.0);

        // reset smoothed controls to current targets
        mDriveLP = mDrive;
        mSizeLP  = mSize;
        mIronLP  = mIron;
        mLoadLP  = mLoad;
        mMixLP   = mMix;
    }

    // ----- Controls (0..1 unless noted) -----
    void setDrive(T v)  { mDrive = std::max(T(0.0), v); }              // >=0 (1 = nominal)
    void setSize(T v)   { mSize  = std::clamp(v, T(0.0), T(1.0)); }      // bigger core
    void setIron(T v)   { mIron  = std::clamp(v, T(0.0), T(1.0)); updateMemoryCoeffs(); }
    void setLoad(T v)   { mLoad  = std::clamp(v, T(0.0), T(1.0)); }      // dynamic headroom/compression
    void setMix(T v)    { mMix   = std::clamp(v, T(0.0), T(1.0)); }      // dry/wet

    // LF/HF split corner (Hz) - where the "core" starts dominating
    void setLFCornerHz(T hz)
    {
        mLFCorner = std::clamp(hz, T(10.0), T(0.45) * mSR);
        updateSplit();
    }

    // Subtle phase smear (0..1). 0 = off.
    void setPhaseSmear(T v)
    {
        mPhase = std::clamp(v, T(0.0), T(1.0));
        updatePhase();
    }

    // Optional internal DC reject (0..1). 0 = off, 1 = stronger.
    void setDCReject(T v01)
    {
        mDCReject = std::clamp(v01, T(0.0), T(1.0));
    }

    // ----- Parameter modulators (scalar) -----
    void setDriveMod(T v) { driveMod.set(v); }  // typically [-1,1]
    void setSizeMod(T v)  { sizeMod.set(v);  }
    void setIronMod(T v)  { ironMod.set(v);  }
    void setLoadMod(T v)  { loadMod.set(v);  }
    void setMixMod(T v)   { mixMod.set(v);   }

    // ----- Audio modulators -----
    void setIM(T v) { im.set(v); }
    void setAM(T v) { am.set(v); }

    // Debug/meters
    T fluxState() const { return mFlux; }
    T hysState()  const { return mHys;  }
    T envState()  const { return mEnv;  }

    T process(T x) override
    {
        // Audio-rate input modulation / gain staging
        const T dry = x;
        x = im.process(x);

        // --- Smooth controls (zipper-safe) ---
        // Modulators are assumed to be scalar signals (no-arg process) in [-1,1].
        T drv = mDrive * (T(1.0) + driveMod.process());
        T siz = mSize  + sizeMod.process();
        T irn = mIron  + ironMod.process();
        T lod = mLoad  + loadMod.process();
        T mix = mMix   + mixMod.process();

        drv = std::max(T(0.0), drv);
        siz = std::clamp(siz, T(0.0), T(1.0));
        irn = std::clamp(irn, T(0.0), T(1.0));
        lod = std::clamp(lod, T(0.0), T(1.0));
        mix = std::clamp(mix, T(0.0), T(1.0));

        // One-pole smoothing (fast enough for automation, slow enough to kill zipper)
        // If you want “analog motion” you can make rise/fall different later.
        mDriveLP += mCtlAlpha * (drv - mDriveLP);
        mSizeLP  += mCtlAlpha * (siz - mSizeLP);
        mIronLP  += mCtlAlpha * (irn - mIronLP);
        mLoadLP  += mCtlAlpha * (lod - mLoadLP);
        mMixLP   += mCtlAlpha * (mix - mMixLP);

        // If Iron is moving, update memory coefficients lazily
        if (std::fabs(mIronLP - mIronCached) > T(1e4))
        {
            mIronCached = mIronLP;
            updateMemoryCoeffsFromIron(mIronLP);
        }

        // --- Drive into transformer ---
        T in = x * mDriveLP;

        // Envelope (drives dynamic headroom and copper loss)
        const T a = std::fabs(in);
        mEnv += (a - mEnv) * mEnvAlpha;

        // --- LF/HF split (simple 1-pole LP) ---
        // LP: y += g*(x - y)
        mLP += mSplitG * (in - mLP);
        T lf = mLP;
        T hf = in - lf;

        // --- Core / flux model (LF-dominant) ---
        // Core "size" maps to headroom. Bigger core -> more headroom.
        // Load reduces headroom dynamically with energy (program-dependent compression).
        const T baseHead = T(0.35) + T(1.65) * mSizeLP;              // ~0.35..2.0
        const T loadComp = T(1.0) / (T(1.0) + (T(0.8) * mLoadLP) * mEnv);
        T head = std::max(T(0.05), baseHead * loadComp);

        // Flux integrates LF energy with leak (recovery). More iron -> slower recovery & more memory.
        // Use a bounded integrator to avoid runaway DC drift.
        const T fluxIn = lf * mFluxDrive;
        mFlux = mFlux * mFluxLeak + fluxIn * (T(1.0) - mFluxLeak);
        mFlux = softLimit(mFlux, T(3.0)); // keep it bounded

        // Hysteresis follows flux with additional lag, adding "magnetic memory"
        mHys = mHys * mHysLeak + mFlux * (T(1.0) - mHysLeak);

        // Hysteresis injection into the core drive (adds asymmetry & thickness)
        const T hysAmt = T(0.05) + T(0.65) * mIronLP;               // ~0.05..0.70
        T coreDrive = lf + hysAmt * mHys;

        // Core saturation (smooth, feedback-safe)
        T lfSat = softSat(coreDrive, head);

        // --- HF behavior (copper loss / HF softening under drive) ---
        // Under higher energy and load, HF loses a bit (transformers tend to soften top on hit).
        const T hfLoss = (T(0.02) + T(0.20) * mLoadLP) * (T(0.25) + T(0.75) * mEnv);
        hf *= (T(1.0) - std::clamp(hfLoss, T(0.0), T(0.35)));

        // Recombine
        T y = lfSat + hf;

        // --- Subtle phase smear (2 cascaded allpass) ---
        if (mPhase > T(0.0))
        {
            y = allpass(y, mAP_a1, mAPz1);
            y = allpass(y, mAP_a2, mAPz2);
        }

        // --- Optional DC reject (very gentle HP) ---
        if (mDCReject > T(0.0))
        {
            // leaky highpass: y = x - z + r*z
            const T r = T(0.995) + (T(1.0) - mDCReject) * T(0.0045); // ~0.995..0.9995
            const T out = y - mDCZ + r * mDCZ;
            mDCZ = y;
            y = out;
        }

        // Dry/Wet
        y = dry + (y - dry) * mMixLP;

        // Output modulation
        return am.process(y);
    }

    // Public modulators (match your style)
    Modulator driveMod, sizeMod, ironMod, loadMod, mixMod;
    Modulator im, am;

private:
    static inline T softSat(T x, T head)
    {
        // tanh saturation with adjustable headroom
        const T g = T(1.0) / std::max(T(1e6), head);
        return head * std::tanh(x * g);
    }

    static inline T softLimit(T x, T lim)
    {
        // Smooth limiter using tanh (C∞), keeps state bounded
        const T g = T(1.0) / std::max(T(1e6), lim);
        return lim * std::tanh(x * g);
    }

    static inline T allpass(T x, T a, T& z)
    {
        // 1st-order allpass: y = -a*x + z; z = x + a*y
        const T y = -a * x + z;
        z = x + a * y;
        return y;
    }

    void updateSplit()
    {
        // 1-pole LP coefficient
        // g = 1 - exp(-2*pi*fc/fs)
        const T fc = std::max(T(10.0), mLFCorner);
        const T x = -T(2.0) * T(3.14159265358979323846) * fc / mSR;
        mSplitG = T(1.0) - std::exp(x);
        mSplitG = std::clamp(mSplitG, T(0.00001), T(0.99999));
    }

    void updatePhase()
    {
        // Map phase smear amount into gentle allpass coefficients.
        // Small a => small phase shift. Keep stable: |a|<1.
        const T amt = mPhase;
        mAP_a1 = T(0.05) + T(0.35) * amt;       // ~0.05..0.40
        mAP_a2 = T(0.03) + T(0.22) * amt;       // ~0.03..0.25
        mAP_a1 = std::clamp(mAP_a1, T(0.0), T(0.95));
        mAP_a2 = std::clamp(mAP_a2, T(0.0), T(0.95));
    }

    void updateMemoryCoeffs()
    {
        updateMemoryCoeffsFromIron(mIron);
    }

    void updateMemoryCoeffsFromIron(T iron01)
    {
        // Iron controls recovery times:
        // flux:  ~20ms .. 900ms
        // hys :  ~120ms .. 3000ms
        const T fluxMs = T(20.0)  + T(880.0)  * iron01;
        const T hysMs  = T(120.0) + T(2880.0) * iron01;

        mFluxLeak = std::exp(-T(1.0) / (T(0.001) * fluxMs * mSR));
        mHysLeak  = std::exp(-T(1.0) / (T(0.001) * hysMs  * mSR));

        // How strongly LF drives flux. More iron -> slightly more “magnetization”.
        mFluxDrive = T(0.65) + T(0.55) * iron01; // ~0.65..1.20

        // Control smoothing alpha (fixed automation safety; can be made program-dependent)
        // ~5ms smoothing
        const T tau = T(0.005);
        mCtlAlpha = T(1.0) - std::exp(-T(1.0) / (tau * mSR));

        // Envelope follower: fast attack, slower release feel (single alpha approx)
        // ~15ms-ish
        const T etau = T(0.015);
        mEnvAlpha = T(1.0) - std::exp(-T(1.0) / (etau * mSR));
    }

private:
    T mSR = T(48000.0);

    // User controls
    T mDrive = T(1.0);
    T mSize  = T(0.6);
    T mIron  = T(0.5);
    T mLoad  = T(0.5);
    T mMix   = T(1.0);

    T mLFCorner = T(180.0);
    T mPhase    = T(0.25);
    T mDCReject = T(0.0);

    // Smoothed controls
    T mDriveLP = T(1.0);
    T mSizeLP  = T(0.6);
    T mIronLP  = T(0.5);
    T mLoadLP  = T(0.5);
    T mMixLP   = T(1.0);

    T mIronCached = -T(1.0);

    // Split
    T mSplitG = T(0.01);
    T mLP     = T(0.0);

    // Core state
    T mFlux = T(0.0);
    T mHys  = T(0.0);
    T mEnv  = T(0.0);

    // Memory coeffs
    T mFluxLeak  = T(0.999);
    T mHysLeak   = T(0.9995);
    T mFluxDrive = T(1.0);

    // Smoothing/envelope coeffs
    T mCtlAlpha = T(0.02);
    T mEnvAlpha = T(0.01);

    // Phase smear (allpass)
    T mAP_a1 = T(0.15);
    T mAP_a2 = T(0.10);
    T mAPz1  = T(0.0);
    T mAPz2  = T(0.0);

    // Optional DC reject state
    T mDCZ = T(0.0);
};

// -----------------------------------------------------------------------------
// PowerSupplySagStage (drop-in like TriodeTube)
// - Virtual PSU sag + recovery (global constraint / "glue")
// - Program-dependent headroom reduction + bias shift
// - Feedback-safe (smooth tanh / soft-limit), no hard corners
// - No allocations, minimal state
//
// Musical controls (0..1 unless noted):
//   Sag        : how easily supply collapses (depth)
//   Cap        : recovery time / "cap size" (bigger = slower recovery)
//   Load       : how strongly audio draws power (sensitivity)
//   BiasCouple : how much sag shifts symmetry (even harmonics / operating point)
//   Mix        : dry/wet
//   BaseHeadroom (>=0.05): nominal headroom scaling applied to saturation
//
// Modulation:
// - im/am are audio modulators (same convention as your FrequencyShifter)
// - sagMod/capMod/loadMod/biasMod/mixMod are scalar modulators (no-arg process)
//
// Typical placement:
//   TriodeTube -> MagneticTransformerStage -> PowerSupplySagStage -> ...
// Or inside feedback paths to add "amp-like" breathing and stabilization.
// -----------------------------------------------------------------------------
class PowerSupplySagStage : public Function
{
public:
    PowerSupplySagStage()
    {
        reset();
        setSampleRate((T)gam::sampleRate());

        setSag(T(0.55));
        setCap(T(0.55));
        setLoad(T(0.55));
        setBiasCouple(T(0.35));
        setMix(T(1.0));

        setBaseHeadroom(T(1.0));
        setHFSoftening(T(0.25));
        setDCReject(T(0.0));

        // PSU corner defaults (envelope characteristics)
        setAttackMs(T(15.0));
        setReleaseMs(T(450.0));
    }

    void setSampleRate(T sr)
    {
        mSR = std::max(T(8000.0), sr);
        updateEnvCoeffs();
        updateCapCoeffs();
        updateHF();
        updateCtlSmoothing();
    }

    void reset()
    {
        mEnv = T(0.0);
        mVpsu = T(1.0);   // normalized supply voltage
        mBiasShift = T(0.0);

        mLP = T(0.0);
        mDCZ = T(0.0);

        mSagLP = mSag;
        mCapLP = mCap;
        mLoadLP = mLoad;
        mBiasCoupleLP = mBiasCouple;
        mMixLP = mMix;

        mCapCached = -T(1.0);
        mLastOut = T(0.0);
    }

    // ----- Controls -----
    void setSag(T v)        { mSag = std::clamp(v, T(0.0), T(1.0)); }          // depth
    void setCap(T v)        { mCap = std::clamp(v, T(0.0), T(1.0)); updateCapCoeffs(); } // recovery
    void setLoad(T v)       { mLoad = std::clamp(v, T(0.0), T(1.0)); }         // sensitivity
    void setBiasCouple(T v) { mBiasCouple = std::clamp(v, T(0.0), T(1.0)); }   // asymmetry from sag
    void setMix(T v)        { mMix = std::clamp(v, T(0.0), T(1.0)); }          // dry/wet

    void setBaseHeadroom(T v) { mBaseHead = std::max(T(0.05), v); }          // >=0.05
    void setHFSoftening(T v)  { mHFSoft = std::clamp(v, T(0.0), T(1.0)); updateHF(); }

    // Optional internal DC reject (you can also use BlockDC externally)
    void setDCReject(T v01) { mDCReject = std::clamp(v01, T(0.0), T(1.0)); }

    // Envelope characteristics (ms)
    void setAttackMs(T ms)  { mAtkMs = std::clamp(ms, T(0.1), T(200.0)); updateEnvCoeffs(); }
    void setReleaseMs(T ms) { mRelMs = std::clamp(ms, T(1.0), T(5000.0)); updateEnvCoeffs(); }

    // ----- Scalar modulation depths (typically [-1,1]) -----
    void setSagMod(T v)        { sagMod.set(v); }
    void setCapMod(T v)        { capMod.set(v); }
    void setLoadMod(T v)       { loadMod.set(v); }
    void setBiasCoupleMod(T v) { biasMod.set(v); }
    void setMixMod(T v)        { mixMod.set(v); }

    // ----- Audio modulators -----
    void setIM(T v) { im.set(v); }
    void setAM(T v) { am.set(v); }

    // Debug/meters
    T envState()  const { return mEnv; }
    T vpsuState() const { return mVpsu; }
    T biasState() const { return mBiasShift; }

    T process(T x) override
    {
        const T dry = x;
        x = im.process(x);

        // --- Modulated parameters ---
        T sag  = mSag        + sagMod.process();
        T cap  = mCap        + capMod.process();
        T load = mLoad       + loadMod.process();
        T bc   = mBiasCouple + biasMod.process();
        T mix  = mMix        + mixMod.process();

        sag  = std::clamp(sag,  T(0.0), T(1.0));
        cap  = std::clamp(cap,  T(0.0), T(1.0));
        load = std::clamp(load, T(0.0), T(1.0));
        bc   = std::clamp(bc,   T(0.0), T(1.0));
        mix  = std::clamp(mix,  T(0.0), T(1.0));

        // --- Smooth controls (automation-safe) ---
        mSagLP        += mCtlAlpha * (sag  - mSagLP);
        mCapLP        += mCtlAlpha * (cap  - mCapLP);
        mLoadLP       += mCtlAlpha * (load - mLoadLP);
        mBiasCoupleLP += mCtlAlpha * (bc   - mBiasCoupleLP);
        mMixLP        += mCtlAlpha * (mix  - mMixLP);

        // Cap changes time constants; update lazily
        if (std::fabs(mCapLP - mCapCached) > T(1e4))
        {
            mCapCached = mCapLP;
            updateCapCoeffsFromCap(mCapLP);
        }

        // --- Envelope follower (power draw proxy) ---
        // Use |x| with attack/release (fast attack, slow release)
        const T ax = std::fabs(x);
        const T eAlpha = (ax > mEnv) ? mEnvAtkA : mEnvRelA;
        mEnv += eAlpha * (ax - mEnv);

        // --- Sag mapping ---
        // Make sag nonlinear: small env -> gentle, big env -> much stronger
        // "load" increases sensitivity; "sag" sets depth.
        const T sens = T(0.6) + T(3.0) * mLoadLP;           // ~0.6..3.6
        const T draw = T(1.0) - std::exp(-sens * mEnv);   // 0..~1

        // Target PSU voltage (bounded)
        // More sag -> lower Vpsu under load.
        T vTarget = T(1.0) - (T(0.85) * mSagLP) * draw;

        // Prevent collapsing to zero (real supplies bottom out but not instantly)
        vTarget = std::clamp(vTarget, T(0.25), T(1.05));

        // --- Recovery dynamics (capacitor behavior) ---
        // Deep sag recovers slower than shallow sag (program-dependent).
        const T depth = T(1.0) - mVpsu;
        const T slowFactor = T(1.0) + T(2.5) * depth;     // more sag => slower recovery

        // One-pole toward target; different rates depending on direction:
        // - sag down is faster
        // - recovery up is slower (cap)
        const T aDown = mSagDownA;
        const T aUp   = std::clamp(mSagUpA / slowFactor, T(0.000001), T(0.2));

        const T aV = (vTarget < mVpsu) ? aDown : aUp;
        mVpsu += aV * (vTarget - mVpsu);

        // --- Bias shift from sag (even harmonics / operating point movement) ---
        // When supply drops, operating point tends to skew and "thicken".
        // Keep it small and smooth.
        const T sagAmt = (T(1.0) - mVpsu); // 0..~0.75
        const T biasT  = (mBiasCoupleLP * T(0.6)) * sagAmt;
        mBiasShift += mBiasA * (biasT - mBiasShift);

        // --- Apply PSU constraint to signal ---
        // 1) Dynamic headroom reduction (starvation)
        const T head = std::max(T(0.05), mBaseHead * (T(0.65) + T(0.70) * mVpsu));
        T y = x;

        // 2) Bias coupling adds subtle asymmetry (even harmonics)
        y += mBiasShift * y * y;

        // 3) Starved saturation: less headroom when Vpsu low
        y = softSat(y, head);

        // 4) HF softening under heavy draw (subtle)
        if (mHFSoft > T(0.0))
        {
            // Simple 1-pole LP on output to emulate supply-induced HF softness.
            // Cutoff shifts down with sag and HFSoft amount.
            // Update g cheaply per sample using a linear approximation around a fixed corner.
            const T sagHF = std::clamp(sagAmt * (T(0.25) + T(0.75) * mHFSoft), T(0.0), T(0.8));

            // Effective LP coefficient increases as sag increases (more smoothing).
            const T g = std::clamp(mHFG + sagHF * T(0.08), T(0.0001), T(0.25));

            mLP += g * (y - mLP);
            // Mix in only a portion so HF doesn't vanish
            y = y * (T(1.0) - T(0.35) * mHFSoft) + mLP * (T(0.35) * mHFSoft);
        }

        // --- Optional DC reject ---
        if (mDCReject > T(0.0))
        {
            const T r = T(0.995) + (T(1.0) - mDCReject) * T(0.0045); // ~0.995..0.9995
            const T out = y - mDCZ + r * mDCZ;
            mDCZ = y;
            y = out;
        }

        // Dry/Wet blend
        y = dry + (y - dry) * mMixLP;

        // Output audio modulation
        return am.process(y);
    }

    // Public modulators (match your style)
    Modulator sagMod, capMod, loadMod, biasMod, mixMod;
    Modulator im, am;

private:
    static inline T softSat(T x, T head)
    {
        // Smooth saturation with headroom scaling
        const T g = T(1.0) / std::max(T(1e6), head);
        return head * std::tanh(x * g);
    }

    void updateCtlSmoothing()
    {
        // ~5ms parameter smoothing
        const T tau = T(0.005);
        mCtlAlpha = T(1.0) - std::exp(-T(1.0) / (tau * mSR));
    }

    void updateEnvCoeffs()
    {
        // Attack/release for envelope follower
        // alpha = 1 - exp(-1/(tau*sr)), tau in seconds
        const T atk = std::max(T(0.0001), mAtkMs * T(0.001));
        const T rel = std::max(T(0.001),  mRelMs * T(0.001));

        mEnvAtkA = T(1.0) - std::exp(-T(1.0) / (atk * mSR));
        mEnvRelA = T(1.0) - std::exp(-T(1.0) / (rel * mSR));
    }

    void updateCapCoeffs()
    {
        updateCapCoeffsFromCap(mCap);
    }

    void updateCapCoeffsFromCap(T cap01)
    {
        // Cap controls sag recovery speed:
        //   cap=0 -> snappy recovery
        //   cap=1 -> slow, gluey recovery
        //
        // Sag-down (droop) is usually faster than recovery.
        // Down: ~2ms..30ms
        // Up  : ~30ms..2500ms
        const T downMs = T(2.0)   + T(28.0)   * cap01;
        const T upMs   = T(30.0)  + T(2470.0) * cap01;

        const T downTau = T(0.001) * downMs;
        const T upTau   = T(0.001) * upMs;

        mSagDownA = T(1.0) - std::exp(-T(1.0) / (downTau * mSR));
        mSagUpA   = T(1.0) - std::exp(-T(1.0) / (upTau   * mSR));

        // Bias shift follows sag fairly slowly (~80ms..700ms)
        const T biasMs = T(80.0) + T(620.0) * cap01;
        const T biasTau = T(0.001) * biasMs;
        mBiasA = T(1.0) - std::exp(-T(1.0) / (biasTau * mSR));

        updateCtlSmoothing();
    }

    void updateHF()
    {
        // Base HF smoothing coefficient for the optional output LP
        // Higher SR -> smaller coefficient for same subjective corner.
        // Choose a nominal corner ~12kHz.
        const T fc = T(12000.0);
        const T x = -T(2.0) * T(3.14159265358979323846) * fc / mSR;
        mHFG = T(1.0) - std::exp(x);
        mHFG = std::clamp(mHFG, T(0.00005), T(0.15));
    }

private:
    T mSR = T(48000.0);

    // Controls
    T mSag        = T(0.55);
    T mCap        = T(0.55);
    T mLoad       = T(0.55);
    T mBiasCouple = T(0.35);
    T mMix        = T(1.0);

    T mBaseHead = T(1.0);
    T mHFSoft   = T(0.25);
    T mDCReject = T(0.0);

    // Envelope timing
    T mAtkMs = T(15.0);
    T mRelMs = T(450.0);

    // Smoothed controls
    T mSagLP = T(0.55);
    T mCapLP = T(0.55);
    T mLoadLP = T(0.55);
    T mBiasCoupleLP = T(0.35);
    T mMixLP = T(1.0);

    T mCtlAlpha = T(0.02);

    // State
    T mEnv = T(0.0);
    T mVpsu = T(1.0);
    T mBiasShift = T(0.0);

    // Optional HF softening state
    T mLP = T(0.0);
    T mHFG = T(0.01);

    // Optional DC reject state
    T mDCZ = T(0.0);

    // Cached
    T mCapCached = -T(1.0);
    T mLastOut = T(0.0);

    // Envelope coefficients
    T mEnvAtkA = T(0.05);
    T mEnvRelA = T(0.005);

    // Sag dynamics coefficients
    T mSagDownA = T(0.05);
    T mSagUpA   = T(0.001);

    // Bias follow coefficient
    T mBiasA = T(0.002);
};


// ------------------------------------------------------------
// ToneCab: Amp-style tone stack + simple cab/speaker coloration
// ------------------------------------------------------------
// Placement: AFTER your oversampled AnalogStage / Waveshaper.
// Runs at base sample-rate (do NOT oversample this).
//
// Knobs are normalized 0..1 unless noted.
//
// Notes on Gamma filters:
// - gam::Biquad uses .level() for gain-ish parameter; exact semantics depend on type.
// - We keep this class robust even if "level" is not dB; you can retune later.
// - We update coefficients at CONTROL RATE (once per block) via ParamLinear ramping.
// ------------------------------------------------------------

class ToneCab : public Function
{
public:
    enum class ToneType { FENDER, MARSHALL, VOX };
    enum class CabType  { OFF, OPEN, CLOSED, BRIGHT };

    ToneCab()
    : toneStack(3),   // default 3 sections; VOX uses 2 but we can just reuse 3 with a neutral 3rd
      cabLP(), cabHP(), postHP(), postLP(),
      cabRes(T(110.0), T(140.0)),
      smear(T(900.0), T(450.0), 3) // subtle phase smear (3 stages)
    {
        // Default smoothing times
        bassS.setTime(T(20.0));
        midS.setTime(T(20.0));
        trebleS.setTime(T(20.0));
        presenceS.setTime(T(20.0));
        cabAmtS.setTime(T(25.0));
        mixS.setTime(T(10.0));
        lowCutS.setTime(T(30.0));
        highCutS.setTime(T(30.0));

        // Attach smoothers to params
        bassP.attachSmoother(&bassS);
        midP.attachSmoother(&midS);
        trebleP.attachSmoother(&trebleS);
        presenceP.attachSmoother(&presenceS);
        cabAmtP.attachSmoother(&cabAmtS);
        mixP.attachSmoother(&mixS);
        lowCutP.attachSmoother(&lowCutS);
        highCutP.attachSmoother(&highCutS);

        // Defaults
        setToneType(ToneType::FENDER);
        setCabType(CabType::OPEN);

        setBass(T(0.5));
        setMid(T(0.5));
        setTreble(T(0.5));
        setPresence(T(0.35));

        setCabAmount(T(0.75));
        setLowCutHz(T(80.0));
        setHighCutHz(T(6500.0));

        setMix(T(1.0));
    }

    // ---------- lifecycle ----------
    void init(T sampleRate)
    {
        sr = std::max(T(8000.0), sampleRate);
        // Gamma uses global sample rate; ensure your engine sets it elsewhere.
        // If not, uncomment:
        // gam::sampleRate(sr);

        nyq = T(0.5) * sr;

        reset();
        forceUpdateAll();
    }

    void reset()
    {
        toneStack.reset();
        cabLP.reset();
        cabHP.reset();
        postHP.reset();
        postLP.reset();
        cabRes.reset();
        smear.reset();

        // reset smoothing to current values (no jump)
        bassS  = ParamLinear(bassP._value,  bassSTimeMs());
        midS   = ParamLinear(midP._value,   midSTimeMs());
        trebleS= ParamLinear(trebleP._value,trebleSTimeMs());
        presenceS=ParamLinear(presenceP._value,presenceSTimeMs());
        cabAmtS= ParamLinear(cabAmtP._value,cabAmtSTimeMs());
        mixS   = ParamLinear(mixP._value,   mixSTimeMs());
        lowCutS= ParamLinear(lowCutP._value,lowCutSTimeMs());
        highCutS=ParamLinear(highCutP._value,highCutSTimeMs());
    }

    // ---------- tone controls ----------
    void setToneType(ToneType t) { toneType = t; dirtyTone = true; }
    void setCabType(CabType t)   { cabType  = t; dirtyCab  = true; }

    void setBass(T v)     { bassP.set(clamp01(v));     dirtyTone = true; }
    void setMid(T v)      { midP.set(clamp01(v));      dirtyTone = true; }
    void setTreble(T v)   { trebleP.set(clamp01(v));   dirtyTone = true; }
    void setPresence(T v) { presenceP.set(clamp01(v)); dirtyTone = true; } // presence = extra HF tilt

    void setCabAmount(T v){ cabAmtP.set(clamp01(v));   dirtyCab  = true; }

    void setLowCutHz(T hz)
    {
        lowCutP.set(std::clamp(hz, T(0.0), T(0.45) * sr));
        dirtyPost = true;
    }

    void setHighCutHz(T hz)
    {
        highCutP.set(std::clamp(hz, T(50.0), T(0.45) * sr));
        dirtyPost = true;
    }

    void setMix(T v) { mixP.set(clamp01(v)); }

    // Optional: set smoothing times (ms)
    void setSmoothingMs(T toneMs, T cabMs, T postMs)
    {
        bassS.setTime(toneMs);
        midS.setTime(toneMs);
        trebleS.setTime(toneMs);
        presenceS.setTime(toneMs);
        cabAmtS.setTime(cabMs);
        lowCutS.setTime(postMs);
        highCutS.setTime(postMs);
        mixS.setTime(std::min(T(10.0), postMs));
    }

    // ---------- processing ----------
    T process(T x) override
    {
        // 1) run smoothers (control-rate updates happen via flags below)
        const T bass     = bassP.process();
        const T mid      = midP.process();
        const T treble   = trebleP.process();
        const T presence = presenceP.process();
        const T cabAmt   = cabAmtP.process();
        const T mix      = mixP.process();
        const T lowCutHz = lowCutP.process();
        const T highCutHz= highCutP.process();

        // 2) Update coefficients when needed (cheap, but keep off per-sample if possible)
        //    If you're processing block-wise, call updateIfNeeded() once per block instead.
        //    Here we keep it safe for sample-by-sample usage:
        if (dirtyTone || bassS.isRamping() || midS.isRamping() || trebleS.isRamping() || presenceS.isRamping())
            updateToneStack(bass, mid, treble, presence);

        if (dirtyCab || cabAmtS.isRamping())
            updateCab(cabAmt);

        if (dirtyPost || lowCutS.isRamping() || highCutS.isRamping())
            updatePost(lowCutHz, highCutHz);

        // 3) DSP chain
        const T dry = x;

        // Tone stack (SOS)
        T y = toneStack.process(x);

        // Cabinet block (can be bypassed)
        y = processCab(y, cabAmt);

        // Post EQ cleanup
        y = processPost(y);

        // Wet/dry mix
        return dry * (T(1.0) - mix) + y * mix;
    }

    void run(const T* in, T* out, size_t n)
    {
        // Block-wise coefficient updates: better than per-sample.
        // Advance smoothers across the block, updating coefficients only when needed.
        for (size_t i = 0; i < n; ++i)
            out[i] = process(in[i]);
    }

private:
    // ---------- Helpers ----------
    static T clamp01(T v) { return std::clamp(v, T(0.0), T(1.0)); }

    static T knobCurve(T v, T p = T(1.6))
    {
        // Pot-like curve; adjust exponent to taste
        v = clamp01(v);
        return std::pow(v, p);
    }

    // Map 0..1 to a "gain factor" around 1.0.
    // This is deliberately conservative because Gamma's .level() behavior varies by filter type.
    static T gainFromKnob(T v, T range = T(1.0))
    {
        // v=0.5 -> 1.0; v=0 -> 1-range; v=1 -> 1+range
        v = clamp01(v);
        return T(1.0) + (T(2.0) * v - T(1.0)) * range;
    }

    // Map 0..1 to a mid-scoop depth (0..1)
    static T scoopFromMid(T mid)
    {
        // Fender: mid knob "fills" scoop; low mid => deep scoop
        mid = clamp01(mid);
        return T(1.0) - knobCurve(mid, T(1.2));
    }

    // Map 0..1 to peak gain strength (0..1.5)
    static T peakFromMid(T mid)
    {
        return T(0.3) + T(1.2) * knobCurve(mid, T(1.1));
    }

    // ---------- Coefficient Updates ----------
    void forceUpdateAll()
    {
        dirtyTone = dirtyCab = dirtyPost = true;

        // Prime once
        updateToneStack(bassP._value, midP._value, trebleP._value, presenceP._value);
        updateCab(cabAmtP._value);
        updatePost(lowCutP._value, highCutP._value);
    }

    void updateToneStack(T bass, T mid, T treble, T presence)
    {
        bass     = knobCurve(bass);
        mid      = knobCurve(mid);
        treble   = knobCurve(treble);
        presence = knobCurve(presence, T(1.3));

        // NOTE: These are "macro" approximations using existing biquads.
        // They are intentionally stable and musical; later you can replace with true WDF stacks.

        // We'll use the SOS sections by configuring each internal Biquad:
        // section[0] = low shelf, section[1] = mid shaping, section[2] = high shelf / presence tilt

        // Ensure we have at least 3
        if (toneStack.mSections.size() < 3)
            toneStack.mSections.resize(3);

        // --- Common base frequencies (tunable) ---
        const T fBassF  = T(120.0);
        const T fMidF   = T(420.0);
        const T fTrebleF= T(3500.0);

        const T fBassM  = T(150.0);
        const T fMidM   = T(700.0);
        const T fTrebleM= T(3200.0);

        const T fBassV  = T(180.0);
        const T fMidV   = T(650.0);
        const T fTrebleV= T(2800.0);

        // --- Gain ranges (conservative) ---
        const T bassRange     = T(0.9);  // +/- 0.9 around 1
        const T trebleRange   = T(1.1);
        const T presenceRange = T(0.5);

        auto& s0 = toneStack.mSections[0];
        auto& s1 = toneStack.mSections[1];
        auto& s2 = toneStack.mSections[2];

        // Default "neutralize" section 2 for VOX if desired (we still use it as presence)
        // Filter types available in Gamma: LOW_SHELF, HIGH_SHELF, PEAKING, BAND_REJECT, BAND_PASS etc.

        switch (toneType)
        {
            case ToneType::FENDER:
            {
                // Low shelf
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassF);
                s0.setRes(T(0.707));
                s0.setLevel(gainFromKnob(bass, bassRange));

                // Mid scoop: band reject amount increases as mid decreases
                s1.setType(gam::BAND_REJECT);
                s1.setFreq(fMidF);
                s1.setRes(T(0.9));
                // "level" here used as strength; tuned by ear
                s1.setLevel(T(1.0) + scoopFromMid(mid) * T(1.2));

                // High shelf (treble) + presence tilt
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleF);
                s2.setRes(T(0.707));
                s2.setLevel(gainFromKnob(treble, trebleRange) * gainFromKnob(presence, presenceRange));
            } break;

            case ToneType::MARSHALL:
            {
                // Tight bass shelf
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassM);
                s0.setRes(T(0.707));
                s0.setLevel(gainFromKnob(bass, T(0.8)));

                // Mid peak (Marshall growl)
                s1.setType(gam::PEAKING);
                s1.setFreq(fMidM);
                s1.setRes(T(1.1));
                s1.setLevel(peakFromMid(mid));

                // Treble shelf + presence
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleM);
                s2.setRes(T(0.707));
                s2.setLevel(gainFromKnob(treble, T(1.2)) * gainFromKnob(presence, T(0.65)));
            } break;

            case ToneType::VOX:
            {
                // Vox is chimey and less mid-scooped; use gentle bass shelf and high shelf.
                s0.setType(gam::LOW_SHELF);
                s0.setFreq(fBassV);
                s0.setRes(T(0.707));
                s0.setLevel(gainFromKnob(bass, T(0.7)));

                // Gentle mid shaping (small peak)
                s1.setType(gam::PEAKING);
                s1.setFreq(fMidV);
                s1.setRes(T(0.8));
                s1.setLevel(T(0.9) + T(0.7) * mid);

                // Treble shelf + presence
                s2.setType(gam::HIGH_SHELF);
                s2.setFreq(fTrebleV);
                s2.setRes(T(0.707));
                s2.setLevel(gainFromKnob(treble, T(1.35)) * gainFromKnob(presence, T(0.8)));
            } break;
        }

        dirtyTone = false;
    }

    void updateCab(T cabAmt)
    {
        cabAmt = clamp01(cabAmt);

        // If cab is OFF, we still keep filters configured but we will bypass in processCab().
        // Cab voice tuning:
        // - OPEN: looser bass, slightly higher HF cut
        // - CLOSED: tighter bass bump, lower HF cut
        // - BRIGHT: higher HF cut, less rolloff, small presence

        T resF = T(110.0);
        T resW = T(160.0);
        T lpF  = T(5600.0);
        T hpF  = T(60.0);

        T smearFreq  = T(900.0);
        T smearWidth = T(500.0);
        T smearDetune= T(0.15);

        switch (cabType)
        {
            case CabType::OFF:
                // values irrelevant, will bypass
                break;

            case CabType::OPEN:
                resF = T(105.0); resW = T(210.0);
                lpF  = T(6000.0);
                hpF  = T(55.0);
                smearFreq = T(900.0); smearWidth = T(650.0);
                smearDetune = T(0.18);
                break;

            case CabType::CLOSED:
                resF = T(130.0); resW = T(140.0);
                lpF  = T(4800.0);
                hpF  = T(70.0);
                smearFreq = T(850.0); smearWidth = T(450.0);
                smearDetune = T(0.12);
                break;

            case CabType::BRIGHT:
                resF = T(115.0); resW = T(170.0);
                lpF  = T(7000.0);
                hpF  = T(65.0);
                smearFreq = T(1100.0); smearWidth = T(520.0);
                smearDetune = T(0.10);
                break;
        }

        // Clamp to SR
        resF = std::clamp(resF,  T(5.0), T(0.45) * sr);
        resW = std::clamp(resW,  T(0.0), T(0.45) * sr);
        lpF  = std::clamp(lpF,  T(50.0), T(0.45) * sr);
        hpF  = std::clamp(hpF,   T(0.0), T(0.45) * sr);

        // Apply
        cabRes.setFreq(resF);
        cabRes.setWidth(resW);

        cabLP.freq(lpF);
        cabLP.res(T(0.707));

        cabHP.freq(hpF);
        cabHP.res(T(0.707));

        smear.freq(smearFreq);
        smear.width(smearWidth);
        smear.setDetune(smearDetune);

        dirtyCab = false;
    }

    void updatePost(T lowCutHz, T highCutHz)
    {
        lowCutHz  = std::clamp(lowCutHz,  T(0.0), T(0.45) * sr);
        highCutHz = std::clamp(highCutHz, T(50.0), T(0.45) * sr);

        if (highCutHz < lowCutHz + T(20.0))
            highCutHz = lowCutHz + T(20.0);

        // HP
        if (lowCutHz <= T(1.0))
        {
            postHpEnabled = false;
        }
        else
        {
            postHpEnabled = true;
            postHP.freq(lowCutHz);
            postHP.res(T(0.707));
        }

        // LP
        if (highCutHz >= T(0.45) * sr)
        {
            postLpEnabled = false;
        }
        else
        {
            postLpEnabled = true;
            postLP.freq(highCutHz);
            postLP.res(T(0.707));
        }

        dirtyPost = false;
    }

    // ---------- DSP blocks ----------
    T processCab(T x, T cabAmt)
    {
        if (cabType == CabType::OFF || cabAmt <= T(1e6))
            return x;

        // Core cab shaping (wet)
        T y = x;

        // Remove rumble a bit before resonance (optional but helps)
        y = cabHP.process(y);

        // Speaker resonance bump
        y = cabRes.process(y);

        // Cone HF rolloff
        y = cabLP.process(y);

        // Subtle phase smear (small effect; too much gets weird)
        const T smearMix = T(0.20);
        T s = smear.process(y);
        y = y * (T(1.0) - smearMix) + s * smearMix;

        // Cab blend
        return x * (T(1.0) - cabAmt) + y * cabAmt;
    }

    T processPost(T x)
    {
        T y = x;
        if (postHpEnabled) y = postHP.process(y);
        if (postLpEnabled) y = postLP.process(y);
        return y;
    }

    // ---------- Smoother time helpers (for reset re-init) ----------
    T bassSTimeMs() const { return T(20.0); }
    T midSTimeMs() const { return T(20.0); }
    T trebleSTimeMs() const { return T(20.0); }
    T presenceSTimeMs() const { return T(20.0); }
    T cabAmtSTimeMs() const { return T(25.0); }
    T mixSTimeMs() const { return T(10.0); }
    T lowCutSTimeMs() const { return T(30.0); }
    T highCutSTimeMs() const { return T(30.0); }

private:
    T sr  = T(48000.0);
    T nyq = T(24000.0);

    ToneType toneType = ToneType::FENDER;
    CabType  cabType  = CabType::OPEN;

    // Parameters + smoothers
    Parameter  bassP, midP, trebleP, presenceP;
    Parameter  cabAmtP, mixP;
    Parameter  lowCutP, highCutP;

    ParamLinear bassS{T(0.5)}, midS{T(0.5)}, trebleS{T(0.5)}, presenceS{T(0.35)};
    ParamLinear cabAmtS{T(0.75)}, mixS{T(1.0)};
    ParamLinear lowCutS{T(80.0)}, highCutS{T(6500.0)};

    // DSP blocks
    SOS toneStack;               // 3 x Biquad sections
    LowPassFilter  cabLP;
    HighPassFilter cabHP;
    Reson          cabRes;
    AllPass2Block  smear;

    HighPassFilter postHP;
    LowPassFilter  postLP;

    bool postHpEnabled = true;
    bool postLpEnabled = true;

    // Dirty flags (coefficient updates)
    bool dirtyTone = true;
    bool dirtyCab  = true;
    bool dirtyPost = true;
};

struct MasterOutput
{
    // user controls
    T outputGainDB = T(0);     // -inf..+12 dB
    bool  normalize   = false;

    // target loudness (approx RMS)
    T targetRMS = T(0.2);

    // meters
    T rms = T(0);
    T peak = T(0);

    // internal
    T gain = T(1);
    T rmsEnv = T(0);

    inline T dBToGain(T dB)
    {
        return std::pow(T(10), dB / T(20));
    }

    void reset()
    {
        rms = peak = T(0);
        rmsEnv = T(0);
        gain = T(1);
    }

    T process(T x)
    {
        // measure
        peak = std::max(peak, std::fabs(x));
        rmsEnv += (x*x - rmsEnv) * T(0.001);
        rms = std::sqrt(rmsEnv);

        // normalization
        if (normalize && rms > T(1e6))
        {
            T targetGain = targetRMS / rms;
            gain += (targetGain - gain) * T(0.001);
        }
        else
        {
            gain = dBToGain(outputGainDB);
        }

        return x * gain;
    }
};

struct DeviceModel
{
    // ===== Device character =====
    T knee        = T(1.0);   // softness of transition
    T symmetry    = T(1.0);   // + side gain
    T asymmetry   = T(1.0);   // - side gain
    T compression = T(0.0);   // envelope-dependent softening
    T biasSense   = T(0.0);   // how strongly bias affects curve
    T hfDamping   = T(0.0);   // suppress HF at high drive

    // ===== Internal state =====
    T driveLP = T(1.0);   // dynamic knee modulation

    // ===== Core transfer (canonical S-curve) =====
    inline T transfer(T x)
    {
        // asymmetry
        if (x >= T(0)) x *= symmetry;
        else          x *= asymmetry;

        // algebraic soft clip (stable, cheap, good harmonics)
        T y = x / std::sqrt(T(1) + knee * x * x);

        return y;
    }

    // ===== Main process =====
    inline T process(T x, T env, T bias)
    {
        // bias affects operating point
        x += bias * biasSense;

        // envelope softens knee (tube-like compression)
        T k = knee * (T(1) + compression * env);

        // asymmetry already applied in transfer
        if (x >= T(0)) x *= symmetry;
        else          x *= asymmetry;

        T y = x / std::sqrt(T(1) + k * x * x);

        return y;
    }
};

struct NonLinearStageDevice
{
    DeviceModel d;

    enum Type {
        TUBE,
        TRANSISTOR,
        DIODE
    };

    TPT1Pole biasLP;
    TPT1Pole sagLP;

    T baseBias   = T(0);
    T biasAmount = T(0);
    T sagAmount  = T(0);

    T bias = T(0);

    NonLinearStage(Type t = TUBE) {
        setType(t);
    }
    void setType(Type t) {
        switch(t) {
            case TUBE: makeTube(); break;
            case DIODE:  makeDiode(); break;
            case TRANSISTOR: makeTransistor(); break;
            default: makeTube(); break;
        }        
    }
    T process(T x, T env, T drive)
    {
        
        T targetBias = baseBias - biasAmount * env;
        bias = biasLP.processLP(targetBias);

        T v = x * drive;

        T y = d.process(v, env, bias);

        T sagTarget = T(1) / (T(1) + sagAmount * env);
        T sag = sagLP.processLP(sagTarget);

        return y * sag;
    }

    void reset()
    {
        biasLP.reset();
        sagLP.reset();
        bias = T(0);
    }

    void makeTube()
    {        
        d.knee        = T(2.0);
        d.symmetry    = T(1.15);
        d.asymmetry   = T(0.85);
        d.compression = T(0.8);
        d.biasSense   = T(1.0);
        d.hfDamping   = T(0.4);
        
    }

    void makeDiode()
    {     
        d.knee        = T(6.0);
        d.symmetry    = T(1.0);
        d.asymmetry   = T(1.0);
        d.compression = T(0.2);
        d.biasSense   = T(0.3);
        d.hfDamping   = T(0.0);
        
    }

    void makeTransistor()
    {     
        d.knee        = T(10.0);
        d.symmetry    = T(1.3);
        d.asymmetry   = T(0.6);
        d.compression = T(0.5);
        d.biasSense   = T(1.5);
        d.hfDamping   = T(0.2);
        
    }
};


class Oversampled : public Function
{
public:
    Oversampled(Function* f)
    : function(f)
    {
        T fs = gam::sampleRate();
        up   = std::make_unique<R8Resampler>(fs, fs * OS, MaxBlock);
        down = std::make_unique<R8Resampler>(fs * OS, fs, MaxBlock);

        inBuf.resize(MaxBlock);
    }

    void setShaper(Function* f) { function = f; }

    T process(T input) override
    {
        if (!function) return input;

        // ---- T -> T ----
        inBuf[0] = static_cast<T>(input);

        // ---- Upsample ----
        auto& hi = up->process(inBuf.data(), 1);

        // ---- Nonlinear processing at high rate ----
        for (T& s : hi)
            s = function->process(static_cast<T>(s));

        // ---- Downsample ----
        auto& lo = down->process(hi.data(), (int)hi.size());

        return lo.empty() ? T(0) : static_cast<T>(lo[0]);
    }

private:
    static constexpr int OS = 4;
    static constexpr int MaxBlock = 256;

    std::unique_ptr<R8Resampler> up;
    std::unique_ptr<R8Resampler> down;
    Function* function = nullptr;

    std::vector<T> inBuf;
};


struct ParameterJitter
{
    // depth and speed of the micro fluctuations
    T amount = T(0.0003);   // how far the value wanders
    T speed  = T(0.03);     // how fast it moves

    T value = T(0);

    inline T rnd()
    {
        return T(2) * (T(rand()) / T(RAND_MAX)) - T(1);
    }

    inline T process()
    {
        // generate new micro target each sample
        T target = rnd() * amount;

        // smooth random motion
        value += speed * (target - value);

        return value;
    }
};


struct PentodeTube
{
    // Controls
    T drive      = T(1.5);   // higher intrinsic gain
    T bias       = T(0.0);   // operating point
    T hardness   = T(0.6);   // knee hardness
    T asymmetry  = T(0.4);   // odd harmonic emphasis
    T screenSag  = T(0.3);   // screen grid compression

    // Internal state
    T env = T(0);
    T memory = T(0);
    
    inline T hardSat(T x)
    {
        return x / sqrtf(T(1) + x * x);
    }

    T process(T x)
    {
        // Pre-gain and bias
        x = x * drive + bias;

        // Envelope follower (screen grid effect)
        T a = fabsf(x);
        env += (a - env) * T(0.02);

        // Screen grid sag reduces gain dynamically
        T sag = T(1) / (T(1) + screenSag * env);
        x *= sag;

        // Memory / hysteresis (less than triode, but present)
        memory += T(0.01) * (x - memory);

        // Asymmetric shaping
        T pos = hardSat(x + asymmetry * memory);
        T neg = hardSat(x - hardness * memory);

        T y = T(0.6) * pos + T(0.4) * neg;

        return y;
    }
};

struct PhaseCorrector
{
    // strength 0..1
    T amount = T(0.5);
    T amountLP = T(0.5);

    // simple allpass-based correction
    T ap_z1 = T(0);
    T ap_z2 = T(0);

    // one-pole allpass core
    inline T allpass(T x, T a)
    {
        T y = -a * x + ap_z1;
        ap_z1 = x + a * y;
        return y;
    }

    T process(T x)
    {
        amountLP += T(0.002) * (amount - amountLP);

        // dynamic coefficient
        T a = T(0.2) + T(0.6) * amountLP;

        // two cascaded allpass stages
        x = allpass(x, a);
        x = allpass(x, a * T(0.7));

        return x;
    }

    void reset()
    {
        ap_z1 = ap_z2 = T(0);
        amountLP = amount;
    }
};

class PowerSag
{
public:
    PowerSag()
    : env(T(4.0)) {}   // very slow response by default

    void prepare()
    {
        env.reset();
    }

    void setAmount(T a)
    {
        amount = std::clamp(a, T(0), T(1));
    }

    void setRecovery(T seconds)
    {
        env.setLag(seconds);
    }

    inline void apply(T input,
                      T& drive,
                      T& outputGain)
    {
        T e = env.process(std::fabs(input));

        // Sag reduces available headroom & drive
        T sag = T(1) - e * T(0.6) * amount;

        drive *= sag;

        // Compensate loudness slightly (feels like tube recovery)
        outputGain *= (T(1) + (T(1) - sag) * T(0.4));
    }

private:
    T amount = T(0);
    EnvFollow env;
};



struct PresenceFilter : public Function
{
    // user control 0..1
    T amount = T(0.5);
    T amountLP = T(0.5);

    // filters
    TPT1Pole airHP;
    TPT1Pole airLP;

    T sr = T(44100);

    void prepare(T sampleRate)
    {
        sr = sampleRate;
        airHP.setHPCut(T(3500), sr);
        airLP.setLPCut(T(15000), sr);
    }

    void reset()
    {
        airHP.reset();
        airLP.reset();
        amountLP = amount;
    }

    T process(T x) override
    {
        // smooth control
        amountLP += T(0.002) * (amount - amountLP);

        // extract "air band"
        T air = airLP.processLP(airHP.processHP(x));

        // gentle presence lift
        x += air * (amountLP * T(0.6));

        return x;
    }
};


struct PsychoacousticExciter : public Function
{
    // user control 0..1
    T amount = T(0.3);
    T amountLP = T(0.3);

    // band isolation
    TPT1Pole highHP;
    TPT1Pole highLP;

    // envelope follower
    T env = T(0);

    T sr = T(44100);

    void prepare(T sampleRate)
    {
        sr = sampleRate;
        highHP.setHPCut(T(5000), sr);
        highLP.setLPCut(T(16000), sr);
    }

    void reset()
    {
        highHP.reset();
        highLP.reset();
        env = T(0);
        amountLP = amount;
    }

    inline T sat(T x)
    {
        return x / (T(1) + std::fabs(x));
    }

    T process(T x) override
    {
        amountLP += T(0.002) * (amount - amountLP);

        // isolate high band
        T high = highLP.processLP(highHP.processHP(x));

        // detect signal energy
        T a = std::fabs(high);
        env += (a - env) * T(0.02);

        // dynamic harmonic synthesis
        T drive = T(1) + env * T(6) * amountLP;
        T exc = sat(high * drive) - high;

        // blend excitation back in
        x += exc * (amountLP * T(0.35));

        return x;
    }
};






class RingMod {
public:
    T depth = T(1.0);

    T process(T carrier, T modulator)
    {
        return carrier * modulator * depth;
    }

    void run(const T* c, const T* m, T* y, size_t n)
    {
        for(size_t i = 0; i < n; ++i)
            y[i] = process(c[i], m[i]);
    }
};

struct SafetyLimiter : public Function
{
    T threshold = T(0.98);   // linear
    T release   = T(0.002);

    T env = T(0);
    T gain = T(1);

    T process(T x) override
    {
        T a = std::fabs(x);

        // envelope follower
        env += (a - env) * T(0.01);

        // compute target gain
        T target = T(1);
        if (env > threshold)
            target = threshold / (env + T(1e12));

        // smooth gain change
        gain += (target - gain) * release;

        return x * gain;
    }
};

struct Saturator
{
    // Controls
    T amount = T(1);        // 0.5 → subtle, 4+ → aggressive
    T curve  = T(1);        // 0.5 → soft, 2+ → sharp
    T asym   = T(0);        // even harmonic bias
    T trim   = T(1);

    // Smoothing
    T amtLP = T(1);
    T curLP = T(1);
    T asymLP = T(0);

    
    // Core transfer
    inline T satCore(T x)
    {
        // soft knee
        T y = x * (T(1) + curLP);
        y = y / (T(1) + fabsf(y));

        // even harmonics
        y += asymLP * y * y;

        // dynamic compression feel
        y = y / (T(1) + T(0.5) * fabsf(y));

        return y;
    }

    // Loudness normalization
    inline T loudComp() const {
        return T(1) / (T(1) + T(0.35) * amtLP);
    }

    inline T process(T x)
    {
        // smooth params
        amtLP  += T(0.002) * (amount - amtLP);
        curLP  += T(0.002) * (curve  - curLP);
        asymLP += T(0.002) * (asym   - asymLP);

        // drive in
        T y = x * amtLP;

        // nonlinear shaping
        y = satCore(y);

        // output trim & level control
        y *= trim * loudComp();

        return y;
    }
};

struct SelfOscillationController
{
    // Controls
    T engage    = T(1);    // how easily it enters oscillation
    T targetAmp = T(0.5);  // desired oscillation amplitude
    T stiffness = T(0.002);// how rigid the control is

    // State
    T oscEnv = T(0);

    inline T process(T fb, T inputEnergy)
    {
        // detect oscillation energy
        T e = fabsf(fb);

        // envelope follower
        oscEnv += T(0.01) * (e - oscEnv);

        // how close we are to oscillation
        T drive = engage * oscEnv;

        // regulate oscillation amplitude
        T error = targetAmp - oscEnv;
        T control = stiffness * error;

        // apply soft correction
        fb += control * fb;

        // allow input to influence oscillation
        fb += T(0.1) * inputEnergy * fb;

        return fb;
    }
};


class Slew
{
public:
    Slew(T value = 0.0) : current(value), target(value) {}

    void setTime(T seconds, T sampleRate)
    {
        const T tau = std::max(1e-6, seconds);
        alpha = 1.0 - std::exp(-1.0 / (tau * sampleRate));
    }

    void setTarget(T value)
    {
        target = value;
    }

    inline T process()
    {
        current += alpha * (target - current);
        return current;
    }

    T get() const { return current; }

private:
    T current = 0.0;
    T target = 0.0;
    T alpha = 0.0;
};

struct SoftClipper
{
    // Controls
    T amount = T(1);     // strength of clipping
    T knee   = T(1);     // 0.5 → soft, 2+ → hard
    T asym   = T(0);     // even harmonic bias
    T trim   = T(1);

    // Smoothing
    T amtLP = T(1);
    T kneeLP = T(1);
    T asymLP = T(0);

    // Core transfer
    inline T clipCore(T x)
    {
        // soft knee
        T y = x * kneeLP;
        y = y / (T(1) + fabsf(y));

        // asymmetry
        y += asymLP * y * y;

        return y;
    }

    // Loudness compensation
    inline T loudComp() const {
        return T(1) / (T(1) + T(0.25) * amtLP);
    }

    inline T process(T x)
    {
        // smooth controls
        amtLP  += T(0.002) * (amount - amtLP);
        kneeLP += T(0.002) * (knee   - kneeLP);
        asymLP += T(0.002) * (asym   - asymLP);

        // drive into clipper
        T y = x * amtLP;

        // nonlinear shaping
        y = clipCore(y);

        // level control
        y *= trim * loudComp();

        return y;
    }
};


struct ThermalNoise
{
    T amount = T(0.00002);   // extremely subtle
    T color  = T(0.2);       // 0=white, 1=brownish
    T level  = T(0);

    // noise filter
    T lp = T(0);

    // RNG
    inline T rnd(){
        return T(2) * (T(rand()) / T(RAND_MAX)) - T(1);
    }

    inline T process()
    {
        T n = rnd() * amount;

        // color the noise (RC thermal behavior)
        lp += color * (n - lp);

        // very slow fluctuation in intensity
        level += T(0.00001) * (rnd() - level);

        return lp * (T(1) + T(0.1) * level);
    }
};

class TonePresence: public Function
{
public:
    
    TonePresence()
    {
        // Tone filters (tilt EQ)
        toneLow.setType(gam::LOW_PASS);
        toneLow.setFreq(T(1200.0));

        toneHigh.setType(gam::HIGH_PASS);
        toneHigh.setFreq(T(1200.0));

        // Presence filter
        presence.setType(gam::BAND_PASS);
        presence.setFreq(T(3500.0));
        presence.setRes(T(0.6));
        presence.setLevel(T(1.0));
    }

    void setTone(T t)
    {
        // t = 0..1
        tone = std::clamp(t, T(0), T(1));
    }

    void setPresence(T p)
    {
        presenceAmt = std::clamp(p, T(0), T(1));
    }

    T process(T x) override
    {
        // --- Tone tilt EQ ---
        T low  = toneLow.process(x);
        T high = toneHigh.process(x);

        T tilt = (T(1) - tone) * low + tone * high;

        // --- Presence lift ---
        T pres = presence.process(tilt);
        return tilt + pres * presenceAmt;
    }

private:
    T tone = T(0.5);
    T presenceAmt = T(0.0);

    OnePole toneLow;
    OnePole toneHigh;
    Biquad  presence;
};

struct FenderBlackfaceToneStack : public ToneStack
{
    // --- Core filters ---
    Biquad bass;      // low shelf
    Biquad midCut;    // mid scoop (always active)
    Biquad midFill;   // mid control
    Biquad treble;   // high shelf

    T makeup = T(0.45);   // Fender stacks lose LOTS of level

    void prepare(T sr) override
    {
        // Bass shelf (~100 Hz)
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(T(100));
        bass.setRes(T(0.707));
        bass.setLevel(T(0));

        // Fixed mid scoop (~400 Hz)
        midCut.setType(gam::PEAKING);
        midCut.setFreq(T(400));
        midCut.setRes(T(0.5));
        midCut.setLevel(-T(10.0));   // always-on scoop

        // Mid control (~650 Hz)
        midFill.setType(gam::PEAKING);
        midFill.setFreq(T(650));
        midFill.setRes(T(0.8));
        midFill.setLevel(T(0));

        // Treble shelf (~4 kHz)
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(T(4000));
        treble.setRes(T(0.707));
        treble.setLevel(T(0));

        makeup = T(0.45);
    }

    void reset() override
    {
        bass.reset();
        midCut.reset();
        midFill.reset();
        treble.reset();
    }

    // bass01, mid01, treble01 ∈ [0,1]
    T process(T x, T bass01, T mid01, T treble01)
    {
        // ----- Control law -----

        // Bass: huge influence (±14 dB)
        T bassDB = -T(14) + T(28) * bass01;

        // Mid knob mostly *fills in* the scoop
        // 0 = deep scoop, 1 = almost flat mids
        T midDB = -T(6) + T(12) * mid01;

        // Treble: glassy top end
        T trebDB = -T(12) + T(24) * treble01;

        bass.setLevel(bassDB);
        midFill.setLevel(midDB);
        treble.setLevel(trebDB);

        // ----- Order matters (Fender topology) -----
        T y = x;
        y = bass.process(y);      // bass first
        y = midCut.process(y);    // fixed scoop
        y = midFill.process(y);   // mid recovery
        y = treble.process(y);    // sparkle last

        return y * makeup;
    }
};


struct VoxACTopBoostToneStack : public ToneStack
{
    // --- Filters ---
    Biquad bass;        // low shelf
    Biquad treble;      // high shelf
    Biquad upperMid;    // fixed nasal peak

    T makeup = T(0.75);   // less loss than Fender, more than Marshall

    void prepare(T sr) override
    {
        // Bass shelf (~120 Hz)
        bass.setType(gam::LOW_SHELF);
        bass.setFreq(T(120));
        bass.setRes(T(0.707));
        bass.setLevel(T(0));

        // Treble shelf (~5.5 kHz)
        treble.setType(gam::HIGH_SHELF);
        treble.setFreq(T(5500));
        treble.setRes(T(0.707));
        treble.setLevel(T(0));

        // Upper-mid nasal peak (~1.6 kHz)
        upperMid.setType(gam::PEAKING);
        upperMid.setFreq(T(1600));
        upperMid.setRes(T(0.6));
        upperMid.setLevel(+T(3.0));

        makeup = T(0.75);
    }

    void reset() override
    {
        bass.reset();
        treble.reset();
        upperMid.reset();
    }

    // bass01, treble01 ∈ [0,1]
    T process(T x, T bass01, T treble01)
    {
        // ----- Control law -----

        // Bass: moderate range (Vox is not bass-heavy)
        T bassDB = -T(8) + T(16) * bass01;

        // Treble: very influential, also affects perceived gain
        T trebleDB = -T(10) + T(20) * treble01;

        bass.setLevel(bassDB);
        treble.setLevel(trebleDB);

        // ----- Filter order (critical for Vox sound) -----
        T y = x;

        y = bass.process(y);        // bass first
        y = upperMid.process(y);   // nasal character
        y = treble.process(y);     // sparkle + bite

        return y * makeup;
    }
};

struct TransientSaturator : public Function
{
    // user control 0..1
    T amount = T(0.5);
    T amountLP = T(0.5);

    // envelope follower
    T env = T(0);

    inline T sat(T x)
    {
        return x / (T(1) + std::fabs(x));
    }

    T process(T x) override
    {
        // smooth control
        amountLP += T(0.002) * (amount - amountLP);

        // envelope follower (fast attack, slow release)
        T a = std::fabs(x);
        env += (a - env) * ((a > env) ? T(0.15) : T(0.01));

        // isolate transient energy
        T transient = a - env;

        // dynamic saturation on transient only
        T shaped = sat(x * (T(1) + transient * T(6) * amountLP));

        // blend: preserve body, enhance attack
        T y = x + (shaped - x) * (transient * T(4) * amountLP);

        return y;
    }
};

struct TransistorModel
{
    // Controls
    T drive = T(1);     // gain into transistor
    T bias  = T(0);     // operating point shift
    T headroom = T(1);  // supply voltage feel
    T trim  = T(1);

    // Smoothing
    T drvLP = T(1);
    T biasLP = T(0);
    T headLP = T(1);

    // Core transistor curve
    inline T transistor(T x)
    {
        // base-emitter knee
        T y = std::tanh(T(2.2) * x);

        // headroom limiting
        y = y / (T(1) + T(0.7) * fabsf(y) / headLP);

        // bias asymmetry
        y += biasLP * y * y;

        return y;
    }

    // Loudness compensation
    inline T loudComp() const {
        return T(1) / (T(1) + T(0.4) * drvLP);
    }

    inline T process(T x)
    {
        // smooth parameters
        drvLP  += T(0.002) * (drive    - drvLP);
        biasLP += T(0.002) * (bias     - biasLP);
        headLP += T(0.002) * (headroom - headLP);

        // drive
        T y = x * drvLP;

        // nonlinear shaping
        y = transistor(y);

        // output
        y *= trim * loudComp();

        return y;
    }
};

class TubeShaper : public Function {
public:
    TubeShaper(T drive=T(1.0), T bias=T(0.0), T curve=T(2.0), T outGain=T(1.0))
    {
        setDrive(drive);
        setBias(bias);
        setCurve(curve);
        setOutGain(outGain);
        setAM(T(1.0));
    }

    // ---------- Controls ----------
    void setDrive(T v)   { mDrive = std::max(T(0), v); m_drive.set(mDrive); }
    void setBias(T v)    { mBias  = std::clamp(v, -T(0.5), T(0.5)); m_bias.set(mBias); }
    void setCurve(T v)   { mCurve = std::max(T(0.01), v); m_curve.set(mCurve); }
    void setOutGain(T v) { mOutGain = std::max(T(0), v); m_gain.set(mOutGain); }
    void setAM(T v)      { m_am.set(v); }

    // ---------- Processing ----------
    T process(T input) override {
        T d = m_drive.process();
        T b = m_bias.process();
        T c = m_curve.process();
        T g = m_gain.process();
        T a = m_am.process();

        d = std::max(T(0), d);
        b = std::clamp(b, -T(0.5), T(0.5));
        c = std::max(T(0.01), c);
        g = std::max(T(0), g);

        // Preamp stage with DC bias
        T x = input * d + b;

        // Asymmetrical nonlinear tube curve
        T y;
        if(x >= T(0)){
            // Softer on positive swing
            y = T(1.0) - std::exp(-x * c);
        } else {
            // Harder on negative swing
            y = - (T(1.0) - std::exp(x * c * T(1.3)));
        }

        // Remove bias-induced DC offset
        y -= b * T(0.6);

        return y * g * a;
    }

    void run(const T* in, T* out, size_t n){
        for(size_t i=0;i<n;i++)
            out[i] = process(in[i]);
    }

    T mDrive   = T(1.0);
    T mBias    = T(0.0);
    T mCurve   = T(2.0);
    T mOutGain = T(1.0);

    Modulator m_drive;
    Modulator m_bias;
    Modulator m_curve;
    Modulator m_gain;
    Modulator m_am;
};


}