// 8x8 Feedback Delay Network Reverb (Hadamard mixing)
// - Uses your Delay wrapper (Gamma underneath) + OnePole damping
// - Uses Comb(allpass) for input/output diffusion (engine style)
// - Uses gam::sampleRate() implicitly via Gamma internals (no SR member)
// - True FDN: cross-coupled feedback via 8x8 Hadamard matrix

class FDNReverb8 : public Function {
public:
    explicit FDNReverb8(float maxDelaySeconds = 1.5f)
    : _maxDelay(maxDelaySeconds)
    , _apIn1(0.10f), _apIn2(0.10f)
    , _apOut1(0.10f), _apOut2(0.10f)
    {
        // Input diffusion (small allpasses)
        _apIn1.setDelay(0.0048f); _apIn1.setAllPass(0.75f);
        _apIn2.setDelay(0.0035f); _apIn2.setAllPass(0.75f);

        // Output diffusion
        _apOut1.setDelay(0.0063f); _apOut1.setAllPass(0.70f);
        _apOut2.setDelay(0.0041f); _apOut2.setAllPass(0.70f);

        // Base delay times (seconds) — incommensurate-ish, spread out.
        // Scale with setSize(). Keep all < maxDelaySeconds.
        _baseTimes = {
            0.0297f, 0.0338f, 0.0371f, 0.0411f,
            0.0437f, 0.0483f, 0.0531f, 0.0601f
        };

        // Init delays + max
        for (int i = 0; i < 8; ++i) {
            _d[i].setMaxDelay(_maxDelay, true);
            _d[i].setIpolType(LINEAR); // good default for modulation-ready tails
        }

        setSize(1.0f);
        setRT60(1.8f);
        setDamping(6500.0f);
        setMix(0.35f);
        setWidth(1.0f);
        setInputGain(0.35f);

        clear();
    }

    // --------- Controls ---------
    void setRT60(float rt60Seconds)
    {
        _rt60 = std::clamp(rt60Seconds, 0.2f, 12.0f);
        updateFeedback();
    }

    void setSize(float s)
    {
        _size = std::clamp(s, 0.25f, 2.5f);
        for (int i = 0; i < 8; ++i) {
            float t = _baseTimes[i] * _size;
            // keep inside max
            t = std::min(t, _maxDelay * 0.95f);
            t = std::max(t, 0.001f);
            _d[i].setDelay(t);
        }
        updateFeedback();
    }

    // Damping is applied inside the feedback loop (tail darkening)
    void setDamping(float hz)
    {
        hz = std::clamp(hz, 50.0f, 18000.0f);
        for (auto& f : _damp) {
            f.setType(OnePole::LOW_PASS);
            f.setFreq(hz);
        }
    }

    void setMix(float m) { _mix = std::clamp(m, 0.0f, 1.0f); }

    // Stereo width: 0 = mono sum, 1 = full decorrelated taps
    void setWidth(float w) { _width = std::clamp(w, 0.0f, 1.0f); }

    // Input injection gain into the network (character / stability)
    void setInputGain(float g) { _inGain = std::max(0.0f, g); }

    void clear()
    {
        for (auto& f : _damp) f.reset(0.0f);
        // best-effort: clear delay state by writing zeros for a short burst
        for (int k = 0; k < 256; ++k) {
            for (int i = 0; i < 8; ++i) _d[i].write(0.0f);
        }
    }

    // --------- Audio ---------
    float process(float x) override
    {
        float L, R;
        process(x, L, R);
        return 0.5f * (L + R);
    }

    void process(float x, float& outL, float& outR)
    {
        // Dry input for mix
        float dry = x;

        // Input diffusion
        float xin = _apIn2.process(_apIn1.process(x));

        // Read delay outputs
        float d[8];
        for (int i = 0; i < 8; ++i) d[i] = _d[i].read();

        // 8x8 Hadamard mix (orthonormal scaling 1/sqrt(8))
        float y[8];
        hadamard8(d, y);

        // Feedback + damping inside the loop + input injection
        for (int i = 0; i < 8; ++i) {
            float fbSig = _damp[i].process(y[i]) * _fb[i];
            _d[i].write(xin * _inGain + fbSig);
        }

        // Stereo output taps:
        // - Use two different sign patterns (also scaled) for decorrelation.
        // - Width controls how different L/R are.
        constexpr float s = 0.3535533905932738f; // 1/sqrt(8)

        float wetL = s * ( d[0] + d[1] - d[2] - d[3] + d[4] + d[5] - d[6] - d[7] );
        float wetR = s * ( d[0] - d[1] + d[2] - d[3] + d[4] - d[5] + d[6] - d[7] );

        // Width blend to mono-wet if desired
        float wetM = 0.5f * (wetL + wetR);
        wetL = wetM * (1.0f - _width) + wetL * _width;
        wetR = wetM * (1.0f - _width) + wetR * _width;

        // Output diffusion
        wetL = _apOut2.process(_apOut1.process(wetL));
        wetR = _apOut2.process(_apOut1.process(wetR));

        // Mix
        outL = dry * (1.0f - _mix) + wetL * _mix;
        outR = dry * (1.0f - _mix) + wetR * _mix;
    }

private:
    // g = 10^(-3 * delay / RT60)  (amplitude)
    static inline float gainFromRT60(float delaySec, float rt60Sec)
    {
        float g = std::pow(10.0f, (-3.0f * delaySec) / rt60Sec);
        return std::clamp(g, 0.0f, 0.99995f);
    }

    void updateFeedback()
    {
        for (int i = 0; i < 8; ++i) {
            float t = _d[i].getDelayTime();
            _fb[i] = gainFromRT60(t, _rt60);
        }
    }

    // Fast 8x8 Hadamard transform (H8 * x), scaled by 1/sqrt(8)
    // Pattern rows:
    //  + + + + + + + +
    //  + - + - + - + -
    //  + + - - + + - -
    //  + - - + + - - +
    //  + + + + - - - -
    //  + - + - - + - +
    //  + + - - - - + +
    //  + - - + - + + -
    static inline void hadamard8(const float x[8], float y[8])
    {
        // Unrolled sums for clarity & speed
        float a0 = x[0] + x[1];
        float a1 = x[0] - x[1];
        float a2 = x[2] + x[3];
        float a3 = x[2] - x[3];
        float a4 = x[4] + x[5];
        float a5 = x[4] - x[5];
        float a6 = x[6] + x[7];
        float a7 = x[6] - x[7];

        float b0 = a0 + a2;
        float b1 = a1 + a3;
        float b2 = a0 - a2;
        float b3 = a1 - a3;

        float b4 = a4 + a6;
        float b5 = a5 + a7;
        float b6 = a4 - a6;
        float b7 = a5 - a7;

        float c0 = b0 + b4;
        float c1 = b1 + b5;
        float c2 = b2 + b6;
        float c3 = b3 + b7;

        float c4 = b0 - b4;
        float c5 = b1 - b5;
        float c6 = b2 - b6;
        float c7 = b3 - b7;

        constexpr float s = 0.3535533905932738f; // 1/sqrt(8)
        y[0] = c0 * s;
        y[1] = c1 * s;
        y[2] = c2 * s;
        y[3] = c3 * s;
        y[4] = c4 * s;
        y[5] = c5 * s;
        y[6] = c6 * s;
        y[7] = c7 * s;
    }

private:
    float _maxDelay = 1.5f;

    // Diffusers
    Comb _apIn1, _apIn2;
    Comb _apOut1, _apOut2;

    // Network
    Delay   _d[8];
    OnePole _damp[8];
    float   _fb[8] = {0};

    std::array<float, 8> _baseTimes;

    // Params
    float _rt60   = 1.8f;
    float _size   = 1.0f;
    float _mix    = 0.35f;
    float _width  = 1.0f;
    float _inGain = 0.35f;
};
