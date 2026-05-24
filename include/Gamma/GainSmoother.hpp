#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

#include "AttackGenerator.hpp"
#include "ReleaseGenerator.hpp" 
#include "AdaptiveTimeConstants.hpp"

namespace gam {

template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class GainSmoother : public Td {
public:
    GainSmoother(){
        reset();
    }

    // ---- base timing ----
    void attack(Tv sec){ baseAtk = sec; }
    void release(Tv sec){ baseRel = sec; }

    // ---- adaptation ----
    void adapt(Tv a){ adap.setAdaptAmount(a); }
    void memory(Tv m){ adap.setMemory(m); }

    // ---- shape ----
    void attackShape(typename AttackGenerator<Tv,Td>::Shape s){
        atk.shape(s);
    }

    void releaseProgram(Tv p){
        rel.program(p);
    }

    void hold(Tv sec){
        rel.hold(sec);
    }

    // ---- reset ----
    void reset(Tv db = Tv(0)){
        atk.reset(db);
        rel.reset(db);
        adap.reset();
        mGainDb = db;
    }

    /// targetGainDb = desired gain reduction (negative dB)
    inline Tv operator()(Tv targetGainDb){
        // Compute adaptive attack & release times
        Tv atkTime, relTime;
        adap.compute(targetGainDb, mGainDb, atkTime, relTime);

        // Apply updated times
        atk.time(atkTime);
        rel.time(relTime);

        // Choose attack or release
        if(targetGainDb < mGainDb){
            // more compression → attack
            mGainDb = atk(targetGainDb);
        } else {
            // releasing
            mGainDb = rel(targetGainDb);
        }

        return mGainDb;
    }

    Tv value() const { return mGainDb; }

    void onDomainChange(double){
        atk.onDomainChange(1.0);
        rel.onDomainChange(1.0);
        adap.onDomainChange(1.0);
    }

private:
    // components
    AttackGenerator<Tv,Td>        atk;
    ReleaseGenerator<Tv,Td>       rel;
    AdaptiveTimeConstants<Tv,Td>  adap;

    // base times
    Tv baseAtk { Tv(0.01) };
    Tv baseRel { Tv(0.1) };

    // state
    Tv mGainDb { Tv(0) };
};

} // namespace gam
