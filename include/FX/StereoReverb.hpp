#include "Reverb.hpp"
#include "EarlyReflections.hpp"

class StereoReverb {
public:
    void prepare(float sr)
    {
        mats = { WOOD, WOOD, CARPET, WOOD, CONCRETE, CONCRETE };

        sampleRate = sr;

        tankL.prepare(sr);
        tankR.prepare(sr);

        er.prepare(sr);
        er.setRoom(6.f, 3.f, 8.f);

        // Default materials
        er.setWallMaterial(0, WOOD);     // left
        er.setWallMaterial(1, WOOD);     // right
        er.setWallMaterial(2, CARPET);   // floor
        er.setWallMaterial(3, WOOD);     // ceiling
        er.setWallMaterial(4, CONCRETE); // front
        er.setWallMaterial(5, CONCRETE); // back

        updateMaterialAverages();
    }

    // ===== Room control =====
    void setRoom(float w,float h,float d){ er.setRoom(w,h,d); updateMaterialAverages(); }
    void setSource(Vec3 s){ er.setSource(s); }
    void setListener(Vec3 l){ er.setListener(l); }
    void setERLevel(float v){ er.setLevel(v); }

    void setWallMaterial(int wall, const WallMaterial& m)
    {
        er.setWallMaterial(wall, m);
        updateMaterialAverages();
    }

    // ===== Reverb controls =====
    void setMode(ReverbMode m){ tankL.setMode(m); tankR.setMode(m); }
    void setSize(float v){ tankL.setSize(v); tankR.setSize(v); }

    void setDecay(float v)
    {
        baseDecay = std::clamp(v, 0.f, 1.f);
    }

    void setDamping(float v)
    {
        userDamping = std::clamp(v, 0.f, 1.f);
    }

    void setPreDelay(float v){ tankL.setPreDelay(v); tankR.setPreDelay(v); }
    void setMix(float v){ mix = std::clamp(v,0.f,1.f); }

    // ===== Audio =====
    void process(float& L, float& R)
    {
        if(dirtyAcoustics)
            rebuildAcoustics();

        float in = 0.5f * (L + R);

        float erL=0.f, erR=0.f;
        er.process(in, erL, erR);

        float wetL = tankL.process(erL);
        float wetR = tankR.process(erR);

        L = (1.f - mix) * L + mix * (erL + wetL);
        R = (1.f - mix) * R + mix * (erR + wetR);
    }

    void setRoom(float w,float h,float d){
        roomW=w; roomH=h; roomD=d;
        er.setRoom(w,h,d);
        dirtyAcoustics = true;
    }

    void setWallMaterial(int wall, const WallMaterial& m){
        er.setWallMaterial(wall,m);
        mats[wall] = m;
        dirtyAcoustics = true;
    }

    void setDecaySeconds(float seconds){
        userRT60 = std::clamp(seconds,0.1f,30.f);
        dirtyAcoustics = true;
    }

    void setDamping(float v){
        userDamping = std::clamp(v,0.f,1.f);
        dirtyAcoustics = true;
    }
    void rebuildAcoustics()
    {
        updateMaterialAverages();
        updateRT60();

        // combine user intent + room physics
        float targetRT60 = 0.5f * userRT60 + 0.5f * rt60Mid;

        tankL.setRT60(targetRT60);
        tankR.setRT60(targetRT60);

        float ratio = std::clamp(rt60High / std::max(0.1f, targetRT60), 0.1f, 2.0f);
        float materialDamping = 1.f - std::clamp(ratio, 0.f, 1.f);

        float finalDamping = std::clamp(0.6f * userDamping + 0.4f * materialDamping, 0.f, 1.f);

        tankL.setDamping(finalDamping);
        tankR.setDamping(finalDamping);

        dirtyAcoustics = false;
    }

private:
    void updateMaterialAverages()
    {
        avgLow = avgMid = avgHigh = 0.f;

        for(int i=0;i<6;i++)
        {
            const WallMaterial& m = er.getWallMaterial(i);
            avgLow  += m.low;
            avgMid  += m.mid;
            avgHigh += m.high;
        }

        avgLow  /= 6.f;
        avgMid  /= 6.f;
        avgHigh /= 6.f;
    }

    void updateRT60()
    {
        RT60Bands rt = computeRT60_Sabine(roomW, roomH, roomD, mats);
        rt60Low  = rt.low;
        rt60Mid  = rt.mid;
        rt60High = rt.high;
    }

    void setDecaySeconds(float seconds)
    {
        userRT60 = std::clamp(seconds, 0.1f, 30.f);
    }

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
