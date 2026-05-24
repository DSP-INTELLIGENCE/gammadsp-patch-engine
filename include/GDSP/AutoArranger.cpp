#include "AutoArranger.hpp"

AutoArranger::AutoArranger(float sr)
: mSampleRate(sr),
  mBeatsPerBar(4),
  mQuantizeBars(1),
  mMinBarsPerSection(4),
  mActiveScene(0),
  mPendingScene(-1),
  mLastSectionSeen(0),
  mBeatCounter(0),
  mBarCounter(0),
  mBarsSinceSwitch(0),
  mLayerCount(4)
{
    // default mapping: section i -> scene i
    for(int i=0;i<32;++i) mSectionToScene[i] = i;

    // default intensity mapping (drums/bass/harm/lead)
    setIntensityToLayers(1.0f, 0.8f, 0.6f, 0.9f);

    reset(0);
}

void AutoArranger::setBeatsPerBar(int n){
    mBeatsPerBar = std::max(1, n);
}

void AutoArranger::setQuantizeBars(int bars){
    mQuantizeBars = std::max(1, bars);
}

void AutoArranger::setMinBarsPerSection(int bars){
    mMinBarsPerSection = std::max(0, bars);
}

void AutoArranger::setSceneForSection(int sectionIndex, int sceneId){
    if(sectionIndex >= 0 && sectionIndex < 32)
        mSectionToScene[sectionIndex] = sceneId;
}

void AutoArranger::setLayerCount(int n){
    mLayerCount = std::clamp(n, 1, 8);
}

void AutoArranger::setIntensityToLayers(float drums, float bass, float harm, float lead){
    // weights for first 4 layers; remaining layers default to 0.5
    mLayerWeights[0] = std::max(0.0f, drums);
    mLayerWeights[1] = std::max(0.0f, bass);
    mLayerWeights[2] = std::max(0.0f, harm);
    mLayerWeights[3] = std::max(0.0f, lead);
    for(int i=4;i<8;++i) mLayerWeights[i] = 0.5f;
}

void AutoArranger::reset(int initialSceneId){
    mActiveScene = initialSceneId;
    mPendingScene = -1;
    mLastSectionSeen = 0;
    mBeatCounter = 0;
    mBarCounter = 0;
    mBarsSinceSwitch = 0;

    for(int i=0;i<8;++i) mLayerGains[i] = 0.0f;

    std::memset(&mCmd, 0, sizeof(mCmd));
    mCmd.switchNow = true;
    mCmd.sceneId = mActiveScene;
    mCmd.pendingSceneId = -1;
    mCmd.intensity = 0.0f;
    for(int i=0;i<4;++i) mCmd.layerGains[i] = 0.0f;
}

bool AutoArranger::isBarStart(const MusicalState& M) const{
    // Bar start when phase wraps near 0. Works even without explicit "beat event".
    // If your BeatTracker exposes a beat event, prefer that for counter updates.
    return (M.phase >= 0.0f && M.phase < 0.02f);
}

const ArrangementCommand& AutoArranger::process(const AnalysisBus& analysis)
{
    const auto& F = analysis.features();
    const auto& M = analysis.musical();
    const auto& E = analysis.expression();

    // --- Intensity (0..1) ---
    // Use expression intensity if available; otherwise map energy into 0..1
    float intensity = std::clamp(E.intensity, 0.0f, 1.0f);

    // Smooth layer gains (simple one-pole)
    for(int i=0;i<mLayerCount;++i){
        float target = std::clamp(intensity * mLayerWeights[i], 0.0f, 1.0f);
        mLayerGains[i] = 0.995f * mLayerGains[i] + 0.005f * target;
    }

    // --- Beat/Bar counting ---
    // We’ll treat "bar start" as the quantization anchor.
    // If your BeatTracker also gives a per-beat event, you can update beatCounter more precisely.
    bool barStart = isBarStart(M);
    if(barStart){
        mBarCounter++;
        mBarsSinceSwitch++;
    }

    // --- Section change request (from SectionDetector) ---
    // Only react if confidence is decent and cooldown has passed.
    int sec = M.section;
    bool sectionChanged = (sec != mLastSectionSeen);
    if(sectionChanged){
        mLastSectionSeen = sec;

        bool allowed = (mBarsSinceSwitch >= mMinBarsPerSection);
        bool confident = (M.sectionConfidence >= 0.35f);

        if(allowed && confident){
            int mappedScene = (sec >= 0 && sec < 32) ? mSectionToScene[sec] : sec;
            if(mappedScene != mActiveScene){
                mPendingScene = mappedScene;
            }
        }
    }

    // --- Quantized switching ---
    bool doSwitch = false;
    if(mPendingScene >= 0 && barStart){
        // QuantizeBars: only switch every N bars (e.g. 2 bars).
        if((mBarCounter % mQuantizeBars) == 0){
            doSwitch = true;
        }
    }

    if(doSwitch){
        mActiveScene = mPendingScene;
        mPendingScene = -1;
        mBarsSinceSwitch = 0;
    }

    // --- Idle behavior example ---
    // If inactive for a while you could schedule a low-energy scene, etc.
    // (Keep this minimal; the engine may handle it elsewhere.)
    (void)F;

    // --- Emit command ---
    mCmd.switchNow = doSwitch;
    mCmd.sceneId = mActiveScene;
    mCmd.pendingSceneId = mPendingScene;
    mCmd.intensity = intensity;

    for(int i=0;i<4;++i){
        mCmd.layerGains[i] = (i < mLayerCount) ? mLayerGains[i] : 0.0f;
    }

    return mCmd;
}
