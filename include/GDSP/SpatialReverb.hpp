// GDSP_SpatialReverb.hpp
class SpatialReverb {
public:
    SpatialReverb(float maxDistDelaySeconds = 0.25f,
            float nearMeters = 0.2f,
            float farMeters  = 25.0f)
    : mDist(maxDistDelaySeconds, nearMeters, farMeters)
    , mLateL(gam::FREEVERB, 0)
    , mLateR(gam::FREEVERB, 23)   // small offset decorrelates the channels
    , mEarly(0.25f, 6)   // max 250ms, 6 reflection taps
    {
        mDist.blockSize(64);

        setSpeedOfSound(343.2f);

        setEarSpacing(0.18f);
        setListener(0.f, 0.f, 0.f);
        setSource(1.f, 0.f, 0.f);

        setLateFlavor(gam::FREEVERB);
        setLateDecay(1.8f);
        setLateDamping(0.3f);
        setLateMix(0.25f);
        setReverbCrossfeed(0.25f);

        setDirectGain(1.0f);
        setWetGain(1.0f);
        setOutputGain(1.0f);
    }

    // ---------- Scene ----------
    void setSpeedOfSound(float mps) { mDist.speedOfSound(mps); }

    void setListener(float x, float y, float z = 0.f) {
        mListenerX = x; mListenerY = y; mListenerZ = z;
        updateDestinations();
    }

    void setSource(float x, float y, float z = 0.f) {
        mSourceX = x; mSourceY = y; mSourceZ = z;
        updateDestinations();
    }

    void setEarSpacing(float meters) {
        mEarSpacing = std::max(0.f, meters);
        updateDestinations();
    }

    // ---------- Distance model ----------
    void setNearFar(float nearMeters, float farMeters) {
        mDist.near(nearMeters);
        mDist.far(farMeters);
        updateDestinations();
    }

    void setFarGain(float g) { mDist.farGain(g); }
    void setToZero(bool b)   { mDist.toZero(b); }
    void setSmoothingBlockSize(int n) { mDist.blockSize(n); }

    // ---------- Late reverb ----------
    void setReverbCrossfeed(float v) {
        // In practice 0..0.35 is a sweet spot, but allow a bit more if desired.
        mReverbCrossfeed = std::clamp(v, 0.f, 0.5f);
    }

    void setLateFlavor(gam::ReverbFlavor flavor, unsigned baseOffset = 0) {
        mLateL.resize(flavor, baseOffset);
        mLateR.resize(flavor, baseOffset + 23);
    }

    void setLateDecay(float seconds) {
        mLateDecay = std::max(0.f, seconds);
        mLateL.decay(mLateDecay);
        mLateR.decay(mLateDecay);
    }

    void setLateDamping(float v) {
        mLateDamping = std::clamp(v, -0.999f, 0.999f);
        mLateL.damping(mLateDamping);
        mLateR.damping(mLateDamping);
    }

    void setLateMix(float m) { mLateMix = std::clamp(m, 0.f, 1.f); }

    // ---------- Gains ----------
    void setDirectGain(float g) { mDirectGain = std::max(0.f, g); }
    void setWetGain(float g)    { mWetGain = std::max(0.f, g); }
    void setOutputGain(float g) { mOutGain = std::max(0.f, g); }

    // ---------- Processing ----------
    inline void process(float in, float& outL, float& outR)
    {
        mSourceX = mSrcX();
        mSourceY = mSrcY();
        mSourceZ = mSrcZ();

        float dx = mSourceX - mListenerX;
        float dy = mSourceY - mListenerY;
        float dz = mSourceZ - mListenerZ;

        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        float vel = (dist - mPrevDist) * mSR;   // meters/sec
        mPrevDist = dist;

        // Doppler factor
        float c = 343.2f;
        float doppler = (c / (c + vel)) * mDopplerScale;

        // 1) Direct field (stereo via two destinations)
        float dopplerIn = in * doppler;
        auto lr = mDist(dopplerIn);

        float dirL = lr[0];
        float dirR = lr[1];

        // 2) True-stereo late reverb: dual tanks + input crossfeed
        float cf = mReverbCrossfeed;
        float revInL = dirL + dirR * cf;
        float revInR = dirR + dirL * cf;

        float wetL = mLateL(revInL);
        float wetR = mLateR(revInR);

        // 3) Mix
        float yL = dirL * mDirectGain + wetL * (mLateMix * mWetGain);
        float yR = dirR * mDirectGain + wetR * (mLateMix * mWetGain);

        // Optional: very gentle soft clip to avoid nasty overloads
        // yL = softClip(yL);
        // yR = softClip(yR);

        outL = yL * mOutGain;
        outR = yR * mOutGain;
    }

    // Primary block API: separate L/R buffers
    void run(const float* input, float* outL, float* outR, size_t n)
    {
        for(size_t i = 0; i < n; ++i) {
            process(input[i], outL[i], outR[i]);
        }
    }

    // Optional compatibility: interleaved LR
    void run(const float* input, float* output, size_t n) override
    {
        for(size_t i = 0; i < n; ++i) {
            float L, R;
            process(input[i], L, R);
            output[2*i]     = L;
            output[2*i + 1] = R;
        }
    }

    void setHeadYaw(float radians) {
        mHeadYaw = radians;
        updateDestinations();
    }

    void setSource(float x, float y, float z=0.f) {
        mSrcX.target(x, 64);
        mSrcY.target(y, 64);
        mSrcZ.target(z, 64);
    }

private:
    // Uncomment if you want safety limiting
    // static inline float softClip(float x) {
    //     // smooth saturation, cheap and stable
    //     return x / (1.f + std::fabs(x));
    // }
    
    gam::Dist<2, float>     mDist;
    gam::ReverbMS<float>    mLateL;
    gam::ReverbMS<float>    mLateR;

    float mReverbCrossfeed = 0.25f;

    float mListenerX = 0.f, mListenerY = 0.f, mListenerZ = 0.f;
    float mSourceX   = 1.f, mSourceY   = 0.f, mSourceZ   = 0.f;
    float mEarSpacing = 0.18f;

    float mLateDecay   = 1.8f;
    float mLateDamping = 0.3f;
    float mLateMix     = 0.25f;

    float mDirectGain  = 1.0f;
    float mWetGain     = 1.0f;
    float mOutGain     = 1.0f;
    float mHeadYaw = 0.f;   // radians
    float mPrevDist = 0.f;
    float mDopplerScale = 1.f;  // user control, default 1
    gam::Ramped<float> mSrcX, mSrcY, mSrcZ;

    inline void updateDestinations()
    {
        const float half = 0.5f * mEarSpacing;

        // Local ear positions before rotation
        float lx = -half, ly = 0.f;
        float rx =  half, ry = 0.f;

        // Rotate ears by head yaw
        float c = std::cos(mHeadYaw);
        float s = std::sin(mHeadYaw);

        auto rot = [&](float& x, float& y){
            float X = c*x - s*y;
            float Y = s*x + c*y;
            x = X; y = Y;
        };

        rot(lx, ly);
        rot(rx, ry);

        // Translate to listener world position
        lx += mListenerX; ly += mListenerY;
        rx += mListenerX; ry += mListenerY;

        auto dist3 = [](float ax,float ay,float az,float bx,float by,float bz){
            float dx=ax-bx, dy=ay-by, dz=az-bz;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        };

        float dL = dist3(mSourceX, mSourceY, mSourceZ, lx, ly, mListenerZ);
        float dR = dist3(mSourceX, mSourceY, mSourceZ, rx, ry, mListenerZ);

        mDist.reset();
        mDist.dist(0, dL);
        mDist.dist(1, dR);
    }
};
