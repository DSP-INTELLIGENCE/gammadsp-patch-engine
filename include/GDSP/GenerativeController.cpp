#include "GenerativeController.hpp"
#include <algorithm>

GenerativeController::GenerativeController()
: mDensity(0), mComplexity(0), mBrightness(0), mExpression(0)
{}

void GenerativeController::reset()
{
    mDensity = mComplexity = mBrightness = mExpression = 0.0f;
}

void GenerativeController::process(float arousal,
                                   float valence,
                                   float tension,
                                   float movement,
                                   int   section,
                                   bool  newSection,
                                   float grooveStability,
                                   float energy,
                                   bool  active)
{
    // Arrangement density
    float densityTarget = std::clamp(arousal * 0.6f + energy * 0.4f, 0.0f, 1.0f);

    // Musical complexity
    float complexityTarget = std::clamp(tension * 0.5f + movement * 0.5f, 0.0f, 1.0f);

    // Brightness / timbral choice
    float brightnessTarget = std::clamp(valence * 0.7f + arousal * 0.3f, 0.0f, 1.0f);

    // Expressiveness
    float expressionTarget = std::clamp((1.0f - grooveStability) * 0.4f + movement * 0.6f, 0.0f, 1.0f);

    // Section logic: emphasize structural changes
    if(newSection)
    {
        densityTarget *= 1.1f;
        complexityTarget *= 1.1f;
    }

    if(!active)
    {
        densityTarget *= 0.2f;
        complexityTarget *= 0.2f;
    }

    // Smooth decision curves
    mDensity     = 0.9f * mDensity     + 0.1f * densityTarget;
    mComplexity  = 0.9f * mComplexity  + 0.1f * complexityTarget;
    mBrightness  = 0.95f * mBrightness + 0.05f * brightnessTarget;
    mExpression  = 0.9f * mExpression  + 0.1f * expressionTarget;
}
