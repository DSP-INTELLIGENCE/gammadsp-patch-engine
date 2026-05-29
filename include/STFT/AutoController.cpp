#include "AutoController.hpp"

AutoController::AutoController(float sr) : mSampleRate(sr) {}

void AutoController::reset()
{
    mLinks.clear();
}

float AutoController::applyCurve(float x, CurveType c)
{
    switch(c) {
        case CURVE_EXP:   return x * x;
        case CURVE_LOG:   return std::sqrt(x);
        case CURVE_SMOOTH:return x * x * (3.0f - 2.0f * x);
        default:          return x;
    }
}

int AutoController::beginLink(float* target,
                              float minOut, float maxOut,
                              float smoothing,
                              CurveType curve,
                              ConditionType cond,
                              bool* active,
                              bool* onset,
                              bool* beat,
                              float* energy)
{
    ModulationLink link;
    link.target = target;
    link.minOut = minOut;
    link.maxOut = maxOut;
    link.smoothing = smoothing;
    link.value = *target;
    link.curve = curve;
    link.condition = cond;
    link.active = active;
    link.onset = onset;
    link.beat = beat;
    link.energy = energy;

    mLinks.push_back(link);
    return (int)mLinks.size() - 1;
}

void AutoController::addSource(int id, float* source, float weight)
{
    auto& L = mLinks[id];
    if(L.sourceCount < 4) {
        L.sources[L.sourceCount++] = { source, weight };
    }
}

void AutoController::setLFO(int id, float rateHz, float depth)
{
    mLinks[id].lfo.rate = rateHz;
    mLinks[id].lfo.depth = depth;
}

void AutoController::setEnvelope(int id, float attack, float decay, float sustain,
                                 bool* trigOnset, bool* trigBeat)
{
    auto& E = mLinks[id].env;
    E.attack = std::max(0.0001f, attack);
    E.decay = std::max(0.0001f, decay);
    E.sustain = std::clamp(sustain, 0.0f, 1.0f);
    mLinks[id].envTriggerOnset = trigOnset;
    mLinks[id].envTriggerBeat = trigBeat;
}

void AutoController::setVelocity(int id, float* velocitySource, float amount)
{
    auto& L = mLinks[id];
    L.velocitySource = velocitySource;
    L.env.velAmount = std::clamp(amount, 0.0f, 1.0f);
}

void AutoController::process()
{
    for(auto& L : mLinks)
    {
        bool allow = true;
        switch(L.condition)
        {
            case COND_WHEN_ACTIVE:      allow = L.active && *L.active; break;
            case COND_WHEN_ONSET:       allow = L.onset && *L.onset; break;
            case COND_WHEN_BEAT:        allow = L.beat && *L.beat; break;
            case COND_WHEN_ENERGY_HIGH: allow = L.energy && *L.energy > 0.6f; break;
            default: break;
        }
        if(!allow) continue;

        float sum = 0.0f, wsum = 0.0f;
        for(int i=0;i<L.sourceCount;i++){
            sum  += (*L.sources[i].value) * L.sources[i].weight;
            wsum += L.sources[i].weight;
        }

        float x = (wsum > 0.0f) ? sum / wsum : 0.0f;
        x = std::clamp(x, 0.0f, 1.0f);
        x = applyCurve(x, L.curve);

        if(L.lfo.depth > 0.0f){
            L.lfo.phase += L.lfo.rate / mSampleRate;
            if(L.lfo.phase >= 1.0f) L.lfo.phase -= 1.0f;
            x += std::sin(6.2831853f * L.lfo.phase) * L.lfo.depth;
        }

        if((L.envTriggerOnset && *L.envTriggerOnset) ||
           (L.envTriggerBeat && *L.envTriggerBeat))
        {
            L.env.active = true;
            float v = 1.0f;
            if(L.velocitySource)
                v = std::clamp(*L.velocitySource, 0.0f, 1.0f);
            L.env.velScale = 1.0f - L.env.velAmount + L.env.velAmount * v;
        }

        if(L.env.active)
        {
            float a = L.env.attack * mSampleRate;
            float d = L.env.decay  * mSampleRate;

            if(L.env.value < 1.0f)
                L.env.value += 1.0f / a;
            else
                L.env.value -= (1.0f - L.env.sustain) / d;

            if(L.env.value <= L.env.sustain)
                L.env.active = false;
        }

        x *= std::clamp(L.env.value * L.env.velScale, 0.0f, 1.0f);
        x = std::clamp(x, 0.0f, 1.0f);

        float targetValue = L.minOut + x * (L.maxOut - L.minOut);
        L.value += (targetValue - L.value) * L.smoothing;
        *L.target = L.value;
    }
}
