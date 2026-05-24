#pragma once

class EmotionMeter {
public:
    
    EmotionMeter()
    : mArousal(0), mValence(0), mTension(0), mMovement(0)
    {}

    void reset()
    {
        mArousal = mValence = mTension = mMovement = 0.0f;
    }

    void process(float energy,
                            float harmonicity,
                            bool  minorKey,
                            float grooveTightness,
                            float grooveSwing,
                            float noisiness)
    {
        // Arousal = energy + motion
        float arousalTarget = std::clamp(energy * 0.7f + grooveTightness * 0.3f, 0.0f, 1.0f);

        // Valence = tonality & mode
        float valenceTarget = harmonicity * (minorKey ? 0.3f : 1.0f);

        // Tension = noise & instability
        float tensionTarget = std::clamp(noisiness * 0.7f + (1.0f - harmonicity) * 0.3f, 0.0f, 1.0f);

        // Movement = groove feel
        float movementTarget = std::clamp(grooveSwing * 0.6f + grooveTightness * 0.4f, 0.0f, 1.0f);

        // Smooth emotional response
        mArousal  = 0.9f * mArousal  + 0.1f * arousalTarget;
        mValence  = 0.95f * mValence + 0.05f * valenceTarget;
        mTension  = 0.9f * mTension  + 0.1f * tensionTarget;
        mMovement = 0.9f * mMovement + 0.1f * movementTarget;
    }

    float arousal() const { return mArousal; }
    float valence() const { return mValence; }
    float tension() const { return mTension; }
    float movement() const { return mMovement; }

private:
    float mArousal;
    float mValence;
    float mTension;
    float mMovement;
};
