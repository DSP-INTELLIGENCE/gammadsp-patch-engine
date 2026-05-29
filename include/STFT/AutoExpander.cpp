#include "AutoExpander.hpp"
#include <algorithm>

AutoExpander::AutoExpander()
{
    reset();
}

void AutoExpander::reset()
{
    mDepth = 0.4f;
}

void AutoExpander::process(float arousal, float tension, float energy)
{
    // Calm sections → more expansion
    // Intense sections → tighter control

    float calm = (1.0f - arousal) * (1.0f - tension);
    float dynamicRoom = calm * (1.0f - energy);

    float targetDepth = std::clamp(0.2f + dynamicRoom * 0.8f, 0.0f, 1.0f);

    mDepth = 0.95f * mDepth + 0.05f * targetDepth;
}
