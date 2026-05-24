template<class Sample>
class FormantTracker {
public:
    explicit FormantTracker(Sample sampleRate)
    : mEnv(sampleRate),
      mFreq(sampleRate),
    {
        mEnv.setAttack(Sample(5));
        mEnv.setRelease(Sample(60));
    }

    void setReactivity(Sample r) { mReactivity = clamp01(r); }
    void setStability(Sample s)  { mStability  = clamp01(s); }
    void setExpressiveness(Sample e) { mExpress = clamp01(e); }

    void connect(FormantFilter* f) { mFilter = f; }

    void process(Sample x)
    {
        const Sample env   = mEnv.process(x);
        const Sample pitch = mFreq.process(x);

        // --- Pitch → vowel estimate ---
        Sample vowelPos = clamp01((pitch - Sample(80)) / Sample(300));

        // --- Envelope articulation ---
        vowelPos += env * mExpress * Sample(0.4);
        vowelPos = clamp01(vowelPos);

        // --- Reactivity / stability shaping ---
        const Sample react = mReactivity * (Sample(1) - mStability);
        const Sample alpha = react * react; // perceptual curve

        vowelPos = lerp(mPrevMorph, vowelPos, alpha);

        // --- Envelope → resonance ---
        Sample sharp = Sample(0.4) + env * Sample(0.8);
        sharp = clamp01(sharp);
s
        // --- Confidence gating ---
        if (mFreq.confidence() >= mConfidenceGate && mFilter) {
            mFilter->setCM(vowelPos);
            mFilter->setRes(sharp);
        }
    }

private:
    static Sample clamp01(Sample x) {
        return std::clamp(x, Sample(0), Sample(1));
    }

    static Sample lerp(Sample a, Sample b, Sample t) {
        return a + (b - a) * t;
    }

private:
    EnvelopeFollower<Sample> mEnv;
    FrequencyDetector<Sample> mFreq;

    Sample mReactivity     = Sample(0.7);
    Sample mStability      = Sample(0.4);
    Sample mExpress        = Sample(0.8);
    Sample mConfidenceGate = Sample(0.4);

    FormantFilter* mFilter = nullptr;
};
