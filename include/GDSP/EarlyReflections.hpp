#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

// You already have these in your project:
#define INC_DELAYS 1
#define INC_FILTERS 1
#include "GammaDSP.hpp" // for Delay, OnePole

static constexpr float kSpeedOfSound = 343.0f; // m/s

struct WallMaterial {
    float low;   // 0..1 reflectivity (bass)
    float mid;   // 0..1 reflectivity (mids)
    float high;  // 0..1 reflectivity (treble)
};

static const WallMaterial CARPET   { 0.30f, 0.15f, 0.05f };
static const WallMaterial WOOD     { 0.80f, 0.60f, 0.45f };
static const WallMaterial GLASS    { 0.95f, 0.85f, 0.70f };
static const WallMaterial CONCRETE { 0.98f, 0.90f, 0.80f };

struct Vec3 {
    float x=0, y=0, z=0;
    Vec3() = default;
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator-(const Vec3& o) const { return {x-o.x,y-o.y,z-o.z}; }
};

static inline float dot(const Vec3& a, const Vec3& b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float len(const Vec3& v){ return std::sqrt(dot(v,v)); }
static inline Vec3  norm(const Vec3& v){
    float l = len(v);
    if(l <= 1e-9f) return {0,0,1};
    return {v.x/l, v.y/l, v.z/l};
}

// Equal-power pan from azimuth [-pi..pi]
static inline void equalPowerPan(float azimuthRad, float& gL, float& gR){
    float pan = 0.5f * (std::sin(azimuthRad) + 1.f); // 0..1
    float a = pan * (float)M_PI_2;
    gL = std::cos(a);
    gR = std::sin(a);
}

class SpatialEarlyReflections {
public:
    enum class ERQuality { Low, Medium, High, Ultra };

    void prepare(float sr){
        sampleRate = sr;
        // Default materials (can override per wall):
        // 0 left, 1 right, 2 floor, 3 ceiling, 4 front, 5 back
        wallMat = { WOOD, WOOD, CARPET, WOOD, CONCRETE, CONCRETE };
        rebuild();
    }

    void reset(){
        for(auto& r : refs){
            r.hp.reset(0.f);
            r.lp.reset(0.f);
        }
    }

    // ===== Room / listener / source =====
    void setRoom(float widthM, float heightM, float depthM){
        roomW = std::max(0.1f, widthM);
        roomH = std::max(0.1f, heightM);
        roomD = std::max(0.1f, depthM);
        rebuild();
    }

    void setSource(Vec3 s){ src = s; rebuild(); }
    void setListener(Vec3 l){ lis = l; rebuild(); }

    // ===== Materials =====
    void setWallMaterial(int wall, const WallMaterial& m){
        if(wall < 0 || wall >= 6) return;
        wallMat[(size_t)wall] = m;
        rebuild();
    }

    // Optional convenience: set all 6 at once
    void setWallMaterials(const std::array<WallMaterial,6>& mats){
        wallMat = mats;
        rebuild();
    }

    // ===== Scaling / quality =====
    void setQuality(ERQuality q){
        quality = q;
        switch(q){
            case ERQuality::Low:
                maxOrder = 1; maxTaps = 8;  maxTime = 0.06f; minEnergy = 0.0020f; break;
            case ERQuality::Medium:
                maxOrder = 2; maxTaps = 16; maxTime = 0.10f; minEnergy = 0.0010f; break;
            case ERQuality::High:
                maxOrder = 3; maxTaps = 32; maxTime = 0.14f; minEnergy = 0.0005f; break;
            case ERQuality::Ultra:
                maxOrder = 4; maxTaps = 64; maxTime = 0.20f; minEnergy = 0.00025f; break;
        }
        rebuild();
    }

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

    // mono in -> stereo ER out
    void process(float in, float& outL, float& outR){
        float yL = 0.f, yR = 0.f;
        for(auto& r : refs){
            float sL = r.dL.process(in);
            float sR = r.dR.process(in);

            sL = r.hp.process(sL);
            sR = r.hp.process(sR);
            sL = r.lp.process(sL);
            sR = r.lp.process(sR);

            yL += sL * r.gL;
            yR += sR * r.gR;
        }
        outL = yL * level;
        outR = yR * level;
    }

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

    void rebuild(){
        refs.clear();

        // Build image sources up to maxOrder with pruning
        std::vector<Img> imgs;
        imgs.reserve(256);
        imgs.push_back({ src, 1.f, -1 });

        for(int order = 1; order <= maxOrder; ++order){
            const int baseCount = (int)imgs.size();
            for(int i = 0; i < baseCount; ++i){
                for(int w = 0; w < 6; ++w){
                    // Optional: avoid immediate “ping-pong” between same wall in consecutive order
                    if(imgs[i].lastWall == w) continue;

                    Img im = imgs[i];
                    im.lastWall = w;

                    // Use mid reflectivity as broadband “importance” for pruning
                    float rMid = std::clamp(wallMat[(size_t)w].mid, 0.f, 1.f);
                    im.energy *= rMid;

                    if(im.energy < minEnergy) continue;

                    // Reflect position across wall
                    reflectAcrossWall(im.pos, w);

                    imgs.push_back(im);
                }
            }
        }

        // Convert images into taps
        refs.reserve((size_t)maxTaps);

        for(const auto& im : imgs){
            Vec3 v = { im.pos.x - lis.x, im.pos.y - lis.y, im.pos.z - lis.z };
            float dist = len(v);
            if(dist < 0.05f) continue;

            float t = dist / kSpeedOfSound;
            if(t > maxTime) continue;

            // Energy rolloff with distance^2
            float invD2 = 1.f / (dist * dist);
            float E = im.energy * invD2;
            if(E < minEnergy) continue;

            addReflectionTap(v, t, E, im.lastWall);
        }

        pruneAndNormalize();
    }

    void reflectAcrossWall(Vec3& p, int w) const {
        switch(w){
            case 0: p.x = -p.x; break;                 // left x=0
            case 1: p.x = 2.f*roomW - p.x; break;      // right x=W
            case 2: p.y = -p.y; break;                 // floor y=0
            case 3: p.y = 2.f*roomH - p.y; break;      // ceiling y=H
            case 4: p.z = -p.z; break;                 // front z=0
            case 5: p.z = 2.f*roomD - p.z; break;      // back z=D
        }
    }

    void addReflectionTap(const Vec3& v, float t, float energy, int wallIndex){
        Vec3 d = norm(v);
        float az = std::atan2(d.x, d.z);

        float pL, pR;
        equalPowerPan(az, pL, pR);

        // ITD
        float itd = (headRadius / kSpeedOfSound) * std::sin(az) * itdAmount;
        float tL = std::max(0.f, t - std::max(0.f, itd));
        float tR = std::max(0.f, t + std::max(0.f, itd));

        // ILD
        float ild = ildAmount * 0.35f * std::sin(az);

        const WallMaterial& m = wallMat[(size_t)std::clamp(wallIndex, 0, 5)];

        // Material mapping:
        // - high reflectivity -> higher LP cutoff (brighter)
        // - low reflectivity -> higher HP cutoff (less boom)
        float lpHz = 800.f + std::clamp(m.high, 0.f, 1.f) * 12000.f;
        float hpHz = 20.f + (1.f - std::clamp(m.low, 0.f, 1.f)) * 120.f;

        // Mid band as overall per-reflection gain (acts like absorption in “presence” band)
        float midGain = std::clamp(m.mid, 0.f, 1.f);

        RefTap tap;
        tap.dL = Delay(sampleRate, maxTime, tL);
        tap.dR = Delay(sampleRate, maxTime, tR);

        tap.hp = OnePole(sampleRate, hpHz, OnePole::HIGH_PASS);
        tap.lp = OnePole(sampleRate, lpHz, OnePole::LOW_PASS);
        tap.hp.lag(0.02f);
        tap.lp.lag(0.02f);

        float base = energy * midGain;
        tap.gL = base * pL * (1.f - ild);
        tap.gR = base * pR * (1.f + ild);

        refs.push_back(std::move(tap));
    }
    
    const WallMaterial& getWallMaterial(int wall) const
    {
        return wallMat[(size_t)std::clamp(wall,0,5)];
    }

    void pruneAndNormalize(){
        // strongest first
        std::sort(refs.begin(), refs.end(),
            [](const RefTap& a, const RefTap& b){
                return (a.gL*a.gL + a.gR*a.gR) > (b.gL*b.gL + b.gR*b.gR);
            });

        if((int)refs.size() > maxTaps)
            refs.resize((size_t)maxTaps);

        // normalize energy
        float sum = 0.f;
        for(const auto& r : refs) sum += std::abs(r.gL) + std::abs(r.gR);
        if(sum > 1e-6f){
            float s = 1.f / sum;
            for(auto& r : refs){ r.gL *= s; r.gR *= s; }
        }
    }
};

/*
How to use materials (examples)
SpatialEarlyReflections er;
er.prepare(sr);

er.setRoom(7.f, 3.f, 10.f);
er.setSource({2.f, 1.5f, 2.f});
er.setListener({3.5f, 1.5f, 5.f});

er.setQuality(SpatialEarlyReflections::ERQuality::High);

// Wood walls, carpet floor, concrete back:
er.setWallMaterial(0, WOOD);
er.setWallMaterial(1, WOOD);
er.setWallMaterial(2, CARPET);
er.setWallMaterial(3, WOOD);
er.setWallMaterial(4, WOOD);
er.setWallMaterial(5, CONCRETE);

*/