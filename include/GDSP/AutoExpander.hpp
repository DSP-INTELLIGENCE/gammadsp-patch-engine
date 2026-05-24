#pragma once

class AutoExpander {
public:
    AutoExpander();

    void reset();

    void process(float arousal, float tension, float energy);

    float depth() const { return mDepth; } // 0..1 (expansion amount)
};
