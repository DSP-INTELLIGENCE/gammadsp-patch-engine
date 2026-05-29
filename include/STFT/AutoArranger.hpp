#pragma once
#include <algorithm>
#include <cstring>
#include "AnalysisBus.hpp"

struct ArrangementCommand {
    bool  switchNow;          // true when a new scene should start this sample
    int   sceneId;            // active scene after switchNow
    int   pendingSceneId;     // scheduled next scene, -1 if none

    float intensity;          // 0..1
    float layerGains[4];      // example: drums, bass, harmony, lead (customize count)
};

// Keep this small & SWIG-friendly
class AutoArranger {
public:
    AutoArranger(float sampleRate);

    // Musical grid
    void setBeatsPerBar(int n);          // typically 4
    void setQuantizeBars(int bars);      // 1 = switch on next bar, 2 = next 2 bars, etc.
    void setMinBarsPerSection(int bars); // minimum time before allowing another section change

    // Mapping: detected section index -> scene id in your engine
    void setSceneForSection(int sectionIndex, int sceneId);

    // Layer behavior (4 layers example; change count if needed)
    void setLayerCount(int n);           // <= 8 recommended
    void setIntensityToLayers(float drums, float bass, float harm, float lead);

    void reset(int initialSceneId = 0);

    // Call once per sample after AnalysisBus has updated
    const ArrangementCommand& process(const AnalysisBus& analysis);

    const ArrangementCommand& command() const { return mCmd; }

private:
    bool isBarStart(const MusicalState& M) const;

    float mSampleRate;

    // grid / quantization
    int mBeatsPerBar;
    int mQuantizeBars;
    int mMinBarsPerSection;

    // section scheduling
    int mActiveScene;
    int mPendingScene;
    int mLastSectionSeen;

    int mBeatCounter;     // counts beats
    int mBarCounter;      // counts bars since last switch
    int mBarsSinceSwitch;

    // layer shaping
    int   mLayerCount;
    float mLayerWeights[8];     // how strongly intensity affects each layer
    float mLayerGains[8];       // smoothed

    ArrangementCommand mCmd;

    // scene map (fixed small)
    int mSectionToScene[32];    // sectionIndex -> sceneId
};
