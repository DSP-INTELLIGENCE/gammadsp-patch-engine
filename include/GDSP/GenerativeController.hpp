#pragma once

class GenerativeController {
public:
    GenerativeController();

    void reset();

    void process(float arousal,
                 float valence,
                 float tension,
                 float movement,
                 int   section,
                 bool  newSection,
                 float grooveStability,
                 float energy,
                 bool  active);

    float density()    const { return mDensity; }
    float complexity() const { return mComplexity; }
    float brightness() const { return mBrightness; }
    float expression() const { return mExpression; }

private:
    float mDensity;
    float mComplexity;
    float mBrightness;
    float mExpression;
};
