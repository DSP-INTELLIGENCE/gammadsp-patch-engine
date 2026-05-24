class FDN8Stereo {
public:
    FDN8Stereo()
    : d{
        Delay(0.5f, 0.0313f), Delay(0.5f, 0.0371f), Delay(0.5f, 0.0411f), Delay(0.5f, 0.0437f),
        Delay(0.5f, 0.0297f), Delay(0.5f, 0.0331f), Delay(0.5f, 0.0397f), Delay(0.5f, 0.0469f)
      }
    {}

    void setFeedback(float g)      { fb = std::clamp(g, -0.999f, 0.999f); }
    void setDamping(float dampHz)  { for (auto& s : sh) s.setDampHz(dampHz); }
    void setBassCut(float hz, float amount){ for (auto& s : sh){ s.setBassCutHz(hz); s.setBassAmount(amount);} }
    void setDrive(float d0)        { for (auto& s : sh) s.setDrive(d0); }

    // 0 = more mono injection, 1 = maximally different injection
    void setStereoSpread(float v)  { stereoSpread = std::clamp(v, 0.0f, 1.0f); }

    // Width is applied after tail; 0=mono, 1=normal, >1 wider
    void setWidth(float w)         { width = std::max(0.0f, w); }

    // Returns stereo tail (late reverb)
    inline void process(float inL, float inR, float& outL, float& outR) {
        // --- read current delay outputs ---
        float y[8];
        for (int i = 0; i < 8; ++i) y[i] = d[i].read();

        // --- frequency-dependent damping + saturation inside loop ---
        for (int i = 0; i < 8; ++i) y[i] = sh[i].process(y[i]);

        // --- Hadamard mix (energy-preserving) ---
        float u[8];
        hadamard8(y, u); // normalized inside

        // --- Stereo injection ---
        // Make different injections to each line so L/R excite the network differently.
        // We inject mid + spread*side with alternating signs.
        float mid  = 0.5f * (inL + inR);
        float side = 0.5f * (inL - inR);

        // scaled side to control how different L/R are in the tank
        float s = stereoSpread * side;

        // injection vector (simple, effective)
        float inj[8] = {
            mid + s, mid - s, mid + s, mid - s,
            mid - s, mid + s, mid - s, mid + s
        };

        // --- write back into the delays (feedback network) ---
        for (int i = 0; i < 8; ++i) {
            d[i].write(inj[i] + fb * u[i]);
        }

        // --- Output mapping (decorrelated taps) ---
        // Sum different subsets/signs to get L/R with low correlation.
        float l = ( y[0] + y[2] + y[5] + y[7] - y[3] );
        float r = ( y[1] + y[4] + y[6] + y[3] - y[2] );

        // normalize rough gain
        l *= 0.2f;
        r *= 0.2f;

        // --- Width via Mid/Side ---
        float M = 0.5f * (l + r);
        float S = 0.5f * (l - r);

        S *= width;

        outL = M + S;
        outR = M - S;
    }

private:
    Delay d[8];
    FeedbackShaper sh[8];

    float fb = 0.75f;
    float stereoSpread = 1.0f;
    float width = 1.0f;

    // Fast normalized Hadamard 8x8.
    // Produces u = (H * y) / sqrt(8)
    static inline void hadamard8(const float in[8], float out[8]) {
        // stage 1
        float a0 = in[0] + in[1]; float a1 = in[0] - in[1];
        float a2 = in[2] + in[3]; float a3 = in[2] - in[3];
        float a4 = in[4] + in[5]; float a5 = in[4] - in[5];
        float a6 = in[6] + in[7]; float a7 = in[6] - in[7];

        // stage 2
        float b0 = a0 + a2; float b1 = a1 + a3;
        float b2 = a0 - a2; float b3 = a1 - a3;
        float b4 = a4 + a6; float b5 = a5 + a7;
        float b6 = a4 - a6; float b7 = a5 - a7;

        // stage 3
        out[0] = b0 + b4;
        out[1] = b1 + b5;
        out[2] = b2 + b6;
        out[3] = b3 + b7;
        out[4] = b0 - b4;
        out[5] = b1 - b5;
        out[6] = b2 - b6;
        out[7] = b3 - b7;

        // normalize by 1/sqrt(8)
        constexpr float norm = 0.3535533905932738f; // 1/sqrt(8)
        for (int i = 0; i < 8; ++i) out[i] *= norm;
    }
};
