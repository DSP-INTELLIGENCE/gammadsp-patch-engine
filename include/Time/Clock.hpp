

// this is a clock
class ModClock : public ModGenerator
{
public:
    enum class Waveform {
        Sweep,
        Sine,
        Saw,
        Square,
        Triangle,
        Pulse
    };

    ModClock(float f = 440.f, Waveform wf = Waveform::Sweep)
        : mWaveform(wf)
    {
        setFreq(f);
        setFM(0);
        setPM(0);
        setAM(1);
        setRatio(1);
        setIndex(1);
    }

    void setFreq(float f)
    {
        f = std::max(0.0f, f);
        baseFreq = f;
        mSweep.setFreq(f);
    }

    void setPhase(float p)
    {
        mSweep.setPhase(p);
    }

    void setWaveform(Waveform wf)
    {
        mWaveform = wf;
    }

    void setPulseWidth(float pw)
    {
        mPulseWidth = std::clamp(pw, 0.01f, 0.99f);
    }

    float process() override
    {
        // First: let ModGenerator apply FM / PM / AM to the phase generator
        float phaseOut = update(mSweep);  
        // `phaseOut` is AM-scaled output of Sweep in [0,1), but we only need raw phase
        return render(phaseOut);
    }

private:
    float render(float p)
    {
        switch (mWaveform)
        {
            case Waveform::Sweep:    return p;
            case Waveform::Sine:     return std::sin(2.0f * M_PI * p);
            case Waveform::Saw:      return 2.0f * p - 1.0f;
            case Waveform::Square:   return p < mPulseWidth ? 1.0f : -1.0f;
            case Waveform::Triangle: return 4.0f * std::fabs(p - 0.5f) - 1.0f;
            case Waveform::Pulse:    return p < mPulseWidth ? 1.0f : 0.0f;
        }
        return 0.0f;
    }

    Sweep    mSweep;
    Waveform mWaveform = Sweep;
    float    mPulseWidth = 0.5f;
};

class ControlEnvelope {
public:

    enum class EnvState {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    
    ControlEnvelope()
        : mState(EnvState::Idle),
            mAttack(0.01f), mDecay(0.1f), mSustain(0.7f), mRelease(0.2f),
            mValue(0.0f)
    {}

    void setADSR(float a, float d, float s, float r) {
        mAttack = std::max(a, 0.0001f);
        mDecay  = std::max(d, 0.0001f);
        mSustain = std::clamp(s, 0.0f, 1.0f);
        mRelease = std::max(r, 0.0001f);
    }

    void noteOn() {
        mState = EnvState::Attack;
        mSweep.setPeriod(mAttack);
        mSweep.reset();
    }

    void noteOff() {
        mState = EnvState::Release;
        mSweep.setPeriod(mRelease);
        mSweep.reset();
        mReleaseStart = mValue;
    }

    float process() {
        switch (mState) {

        case EnvState::Idle:
            return 0.0f;

        case EnvState::Attack: {
            float p = mSweep.process();
            mValue = p;                         // 0 → 1
            if (p >= 1.0f) {
                mState = EnvState::Decay;
                mSweep.setPeriod(mDecay);
                mSweep.reset();
            }
            break;
        }

        case EnvState::Decay: {
            float p = mSweep.process();
            mValue = 1.0f + (mSustain - 1.0f) * p;
            if (p >= 1.0f) {
                mState = EnvState::Sustain;
            }
            break;
        }

        case EnvState::Sustain:
            mValue = mSustain;
            break;

        case EnvState::Release: {
            float p = mSweep.process();
            mValue = mReleaseStart * (1.0f - p);
            if (p >= 1.0f) {
                mState = EnvState::Idle;
                mValue = 0.0f;
            }
            break;
        }
        }

        return mValue;
    }

private:
    Sweep    mSweep;
    EnvState mState;

    float mAttack, mDecay, mSustain, mRelease;
    float mValue;
    float mReleaseStart;
};

class ControlClock {
public:
    ControlClock(float bpm = 120.0f) { setBPM(bpm); }

    void setBPM(float bpm)
    {
        mBPM = bpm;
        mSweep.setFreq(bpm / 60.0f);
    }

    void reset()
    {
        mSweep.reset();
        mLastPhase = 0.0f;
    }

    bool tick()
    {
        float p = mSweep.process();
        bool t = (p < mLastPhase);
        mLastPhase = p;
        return t;
    }

    bool tickDiv(int div)
    {
        float p = mSweep.process();
        bool t = int(p * div) != int(mLastPhase * div);
        mLastPhase = p;
        return t;
    }

private:
    Sweep mSweep;
    float mBPM = 120.0f;
    float mLastPhase = 0.0f;
};

class ControlStepSequencer {
public:
    ControlStepSequencer(int steps = 16)
        : mSteps(steps), mIndex(0)
    {
        mPattern.resize(steps, 0.0f);
    }

    void setStep(int i, float value) {
        if (i >= 0 && i < mSteps) mPattern[i] = value;
    }

    void setSteps(int n) {
        mSteps = n;
        mPattern.resize(n);
        mIndex = 0;
    }

    bool process(bool tick) {
        if (tick) {
            mIndex = (mIndex + 1) % mSteps;
            return true;
        }
        return false;
    }

    float value() const {
        return mPattern[mIndex];
    }

    int index() const { return mIndex; }

private:
    int mSteps;
    int mIndex;
    std::vector<float> mPattern;
};

class ControlWavetable {
public:
    explicit ControlWavetable(size_t size = 2048)
        : mSize(size), mData(size)
    {}

    float* data() { return mData.data(); }
    size_t size() const { return mSize; }

    float sample(float phase) const {
        float pos = phase * mSize;
        int i0 = (int)pos;
        int i1 = (i0 + 1) % mSize;
        float frac = pos - i0;
        return mData[i0] * (1.0f - frac) + mData[i1] * frac;
    }

private:
    size_t mSize;
    std::vector<float> mData;
};

class ControlWavetableOscillator : public Generator {
public:
    ControlWavetableOscillator(ControlWavetable* table, float freq = 440.0f)
        : mTable(table)
    {
        mSweep.setFreq(freq);
    }

    void setFreq(float f) { mSweep.setFreq(f); }
    void setPhase(float p) { mSweep.setPhase(p); }

    float process() override {
        float p = mSweep.process();
        return mTable->sample(p);
    }

    float processFM(float fm) {
        float p = mSweep.processFM(fm);
        return mTable->sample(p);
    }

private:
    Sweep* mSweepPtr = nullptr;
    Sweep  mSweep;
    ControlWavetable* mTable;
};

struct ControlGrain {
    bool active = false;

    Sweep position;
    Sweep envelope;

    float duration = 0.1f;
    float amplitude = 1.0f;
    float startPhase = 0.0f;

    void trigger(float pos, float dur, float amp) {
        active = true;
        duration = dur;
        amplitude = amp;
        startPhase = pos;

        position.setPeriod(dur);
        position.setPhase(pos);
        envelope.setPeriod(dur);
        envelope.reset();
    }

    float process(const Wavetable& source) {
        if (!active) return 0.0f;

        float p = position.process();
        float env = envelope.process();

        if (env >= 1.0f) {
            active = false;
            return 0.0f;
        }

        float sample = source.sample(p);
        float window = std::sin(M_PI * env);   // Hann window
        return sample * window * amplitude;
    }
};

class ControlGranular {
    public:
        Granular(ControlWavetable* source, int maxGrains = 32)
            : mSource(source), mGrains(maxGrains)
        {}
    
        void setDensity(float grainsPerSec) {
            mClock.setFreq(grainsPerSec);
        }
    
        void setDuration(float sec) { mDuration = sec; }
        void setAmplitude(float amp) { mAmplitude = amp; }
    
        float process() {
            bool tick = mClock.process();
    
            if (tick) spawnGrain();
    
            float out = 0.0f;
            for (auto& g : mGrains) {
                out += g.process(*mSource);
            }
            return out;
        }
    
    private:
        void spawnGrain() {
            for (auto& g : mGrains) {
                if (!g.active) {
                    float pos = randomFloat();        // 0..1
                    g.trigger(pos, mDuration, mAmplitude);
                    break;
                }
            }
        }
    
    private:
        ControlWavetable* mSource;
        ControlClock mClock{10.0f};        // grains per second
        std::vector<Grain> mGrains;
    
        float mDuration = 0.1f;
        float mAmplitude = 0.3f;
    };
