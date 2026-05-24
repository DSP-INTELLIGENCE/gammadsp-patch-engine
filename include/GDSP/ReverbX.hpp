
///////////////////////////////////////////////////////////////////
// Reverb/FX
///////////////////////////////////////////////////////////////////
    /*
    ## 🏛️ Tier 1 — The Foundations (must-study)

    These define almost everything that came after.

    ### 1. Lexicon 224 / 480L

    > The sound of modern reverb.

    What to model

    * Lush, dense, *moving* tails
    * Slight modulation inside the tank
    * Huge spaces that never sound metallic
    * Musical high-frequency rolloff

    Why it matters
    Almost every high-end reverb today is spiritually a Lexicon.

    ---

    ### 2. EMT 140 (Plate)

    > The most-recorded reverb ever.

    What to model

    * Bright, dense, fast build-up
    * Short early energy, very smooth tail
    * Beautiful on vocals & drums

    Why it matters
    Defines “plate” sound.

    ---

    ### 3. EMT 250

    > The first digital reverb.

    What to model

    * Thick, warm tail
    * Characterful diffusion
    * Slight coloration that feels expensive

    ---

    ### 4. TC Electronic System 6000 / M5000

    > Surgical + musical.

    What to model

    * Ultra-smooth diffusion
    * Extremely clean decay control
    * Realistic room programs

    ---

    ## 🧪 Tier 2 — Character & Color

    ### 5. Eventide H3000 / H9000

    > Experimental, wide, cinematic.

    What to model

    * Pitch-modulated tails
    * Massive stereo depth
    * Complex routing topologies

    ---

    ### 6. Bricasti M7

    > The modern gold standard.

    What to model

    * Hyper-realistic spaces
    * Perfect early reflection realism
    * Dense tail without modulation artifacts

    ---

    ### 7. Quantec Yardstick

    > The “3D space” machine.

    What to model

    * Extremely natural decay behavior
    * Phase-coherent spatial field
    * Transparent diffusion

    ---

    ### 8. AMS RMX16

    > Iconic 80s character.

    What to model

    * Nonlinear reverb
    * Gated & reverse programs
    * Aggressive early reflections

    ---

    ## 🎬 Tier 3 — Cinematic & Ambient

    ### 9. Lexicon PCM70 / PCM90 / PCM96

    > Film scoring workhorses.

    What to model

    * Endless evolving tails
    * Modulated diffusion
    * Massive but soft ambience

    ---

    ### 10. Valhalla DSP (Modern reference)

    > Software done right.

    What to model

    * Extremely efficient diffusion
    * Smooth RT60 scaling
    * Musical damping behavior

    ---

    ## 🧩 Special Categories

    ### Plates

    * EMT 140
    * UADx Plate
    * Abbey Road RS124 chambers

    ### Rooms & Halls

    * Bricasti M7
    * TC VSS3
    * Lexicon Hall algorithms

    ### Creative / Sound Design

    * Eventide Blackhole
    * Strymon BigSky / NightSky

    ---

    ## 🧠 How to Use This List With Your Engine

    Your engine already supports everything needed to emulate the core behavior of all of these:

    | Unit Feature             | Already in your engine |
    | ------------------------ | ---------------------- |
    | Early reflection realism | ✔                      |
    | RT60 physics             | ✔                      |
    | Dense diffusion          | ✔                      |
    | Modulated tank           | ✔                      |
    | Material damping         | ✔                      |
    | Stereo coherence         | ✔                      |
    | Complex routing          | ✔                      |

    What remains is *voicing*:

    * specific diffusion settings
    * modulation depth
    * damping curves
    * preset geometry
    * spectral tilt

    ---

    If you'd like, next step I can:

    1. Choose one unit (e.g., Lexicon 480L or EMT 140), and
    2. Design a faithful model preset & tuning strategy inside your engine.
    */



#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include "GammaDSP.hpp"

enum class ReverbMode { Plate, Hall, Ambient };

static constexpr float kSpeedOfSound = 343.0f; // m/s

static const WallMaterial CARPET   { 0.30f, 0.15f, 0.05f };
static const WallMaterial WOOD     { 0.80f, 0.60f, 0.45f };
static const WallMaterial GLASS    { 0.95f, 0.85f, 0.70f };
static const WallMaterial CONCRETE { 0.98f, 0.90f, 0.80f };


struct WallMaterial {
    float low;   // 0..1 reflectivity (bass)
    float mid;   // 0..1 reflectivity (mids)
    float high;  // 0..1 reflectivity (treble)
};


struct Vec3 {
    float x=0, y=0, z=0;
    Vec3() = default;
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator-(const Vec3& o) const { return {x-o.x,y-o.y,z-o.z}; }
};

class SpatialEarlyReflections {
public:
    enum class ERQuality { Low, Medium, High, Ultra };

    void prepare(float sr);
    void reset();
    void setRoom(float widthM, float heightM, float depthM);
    void setWallMaterial(int wall, const WallMaterial& m);
    void setWallMaterials(const std::array<WallMaterial,6>& mats)
    void setQuality(ERQuality q);
    void process(float in, float& outL, float& outR);
    
    void setSource(Vec3 s){ src = s; rebuild(); }
    void setListener(Vec3 l){ lis = l; rebuild(); }
    void setMaxTaps(int n){ maxTaps = std::clamp(n, 1, 256); rebuild(); }
    void setMaxOrder(int o){ maxOrder = std::clamp(o, 1, 8); rebuild(); }
    void setMinEnergy(float e){ minEnergy = std::max(0.f, e); rebuild(); }
    void setMaxTime(float seconds){ maxTime = std::clamp(seconds, 0.01f, 0.25f); rebuild(); }

    // ===== Spatial cues =====
    void setHeadRadius(float rMeters){ headRadius = std::clamp(rMeters, 0.05f, 0.12f); rebuild(); }
    void setITDAmount(float a){ itdAmount = std::clamp(a, 0.f, 1.f); rebuild(); }
    void setILDAmount(float a){ ildAmount = std::clamp(a, 0.f, 1.f); rebuild(); }

    // ===== Output level =====
    void setLevel(float v){ level = std::clamp(v, 0.f, 2.f); }

    
private:
    struct RefTap {
        Delay dL, dR;
        OnePole hp;   // HIGH_PASS
        OnePole lp;   // LOW_PASS
        float gL=0.f, gR=0.f;
    };

    struct Img {
        Vec3 pos;
        float energy;        // broadband “importance”
        int lastWall = -1;   // which wall created this image
    };

    float sampleRate = 44100.f;

    float roomW = 6.f, roomH = 3.f, roomD = 8.f;
    Vec3 src {2.f, 1.5f, 2.f};
    Vec3 lis {3.f, 1.5f, 4.f};

    float level = 1.f;
    float maxTime = 0.12f;

    float headRadius = 0.09f;
    float itdAmount = 1.0f;
    float ildAmount = 0.5f;

    // Scaling controls
    int maxOrder = 2;
    int maxTaps  = 16;
    float minEnergy = 0.001f;
    ERQuality quality = ERQuality::Medium;

    // Materials per wall
    std::array<WallMaterial, 6> wallMat { WOOD, WOOD, CARPET, WOOD, CONCRETE, CONCRETE };

    std::vector<RefTap> refs;

    void rebuild();
    void reflectAcrossWall(Vec3& p, int w) const;
    void addReflectionTap(const Vec3& v, float t, float energy, int wallIndex);    
    const WallMaterial& getWallMaterial(int wall) const;
    void pruneAndNormalize();

};


// ---- Your Reverb ----
class ReverbModule {
public:
    void prepare(float sr);

    void setSize(float s);       // 0..1
    void setDecay(float d);      // 0..1
    void setPreDelay(float t);   // seconds
    void setDamping(float d);    // 0..1
    void setMix(float m);        // 0..1
    void setMode(ReverbMode m);

    float process(float input);
    void processBlock(float* buffer, size_t n);

    void reset();
    void applySize();
    void setRT60(float rt60Seconds);

private:
    float sampleRate = 44100.f;

    // Core blocks
    Delay preDelay;
    std::array<Delay, 4> early;

    // Diffusion as allpass chain (better than plain delays)
    std::array<AllPass1, 4> diffusers;

    // Tank delays
    std::array<Delay, 2> tankA;
    std::array<Delay, 2> tankB;

    // Feedback state
    float feedbackA = 0.f;
    float feedbackB = 0.f;

    // Tone shaping inside feedback
    OnePole hpA, hpB;    // high-pass (rumble control)
    OnePole dampA, dampB; // low-pass (HF damping)



    // Params
    float size    = 0.5f;
    float decay   = 0.7f;
    float damping = 0.5f;
    float mix     = 0.5f;
    ReverbMode mode = ReverbMode::Hall;

    // Base times (seconds) before scaling by size
    std::array<float, 4> earlyTimes   { 0.013f, 0.021f, 0.034f, 0.055f };
    std::array<float, 4> diffuserTimes{ 0.011f, 0.017f, 0.031f, 0.043f };
    std::array<float, 2> tankTimesA   { 0.149f, 0.211f };
    std::array<float, 2> tankTimesB   { 0.179f, 0.263f };

    void applySize();
    void tuneMode();
    float processTank(float x);

    static float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

    // in ReverbModule private:
    std::array<float,2> tankTimeA_cur {0.149f, 0.211f};
    std::array<float,2> tankTimeB_cur {0.179f, 0.263f};

    float approxLoopSeconds() const {
        float sum = tankTimeA_cur[0] + tankTimeA_cur[1] + tankTimeB_cur[0] + tankTimeB_cur[1];
        return 0.25f * sum;
    }

};

class StereoReverb {
public:

    void prepare(float sr);

    // ===== Room control =====
    void setRoom(float w,float h,float d){ er.setRoom(w,h,d); updateMaterialAverages(); }
    void setSource(Vec3 s){ er.setSource(s); }
    void setListener(Vec3 l){ er.setListener(l); }
    void setERLevel(float v){ er.setLevel(v); }
    void setWallMaterial(int wall, const WallMaterial& m);
    
    // ===== Reverb controls =====
    void setMode(ReverbMode m){ tankL.setMode(m); tankR.setMode(m); }
    void setSize(float v){ tankL.setSize(v); tankR.setSize(v); }
    void setDecay(float v) { baseDecay = std::clamp(v, 0.f, 1.f); }
    void setDamping(float v) { userDamping = std::clamp(v, 0.f, 1.f); }
    void setPreDelay(float v){ tankL.setPreDelay(v); tankR.setPreDelay(v); }
    void setMix(float v){ mix = std::clamp(v,0.f,1.f); }

    // ===== Audio =====
    
    void process(float& L, float& R);
    
    void setRoom(float w,float h,float d);
    void setWallMaterial(int wall, const WallMaterial& m);
    void setDecaySeconds(float seconds);
    void rebuildAcoustics();

    
private:

    void updateMaterialAverages();
    void updateRT60();
    void setDecaySeconds(float seconds) { userRT60 = std::clamp(seconds, 0.1f, 30.f); }

private:
    float sampleRate = 44100.f;
    float mix = 0.4f;

    float baseDecay  = 0.8f;
    float userDamping = 0.45f;

    float avgLow  = 0.f;
    float avgMid  = 0.f;
    float avgHigh = 0.f;

    // StereoReverb members:
    float roomW=6.f, roomH=3.f, roomD=8.f;
    float userRT60 = 1.8f;      // seconds, "Decay" knob as RT60-mid target
    float rt60Low=1.f, rt60Mid=1.f, rt60High=1.f;
    std::array<WallMaterial,6> mats;

    SpatialEarlyReflections er;
    ReverbModule tankL, tankR;
    bool dirtyAcoustics = true;
};

