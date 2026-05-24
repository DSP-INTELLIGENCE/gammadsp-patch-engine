#pragma once
#include <algorithm>
#include <cmath>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

#include "Gamma/EvenHarmonicColor.hpp"

namespace gam {


// ------------------------------------------------------------
// CompressorColorStage: plugs into your compressor right after GR
// Call: y = color(x, g)
// where g is the compressor's linear gain (0..inf, usually <=1)
// ------------------------------------------------------------
template<class Tv = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class CompressorColorStage : public Td {
public:
    // --- blocks
    EvenHarmonicColor<Tv, Td> even;
    BlockDC<Tv>       dc;

    // --- user params
    void wet(Tv w)        { mWet = std::clamp(w, Tv(0), Tv(1)); }

    // amount of even-harmonic color at “full” GR
    void evenMax(Tv a)    { mEvenMax = std::clamp(a, Tv(0), Tv(1)); }

    // how quickly color ramps in with GR (0.5..2 typical)
    void evenCurve(Tv c)  { mEvenCurve = std::max(Tv(0.01), c); }

    // optional post-color soft clip (0 = off)
    void clipAmount(Tv a) { mClipAmt = std::max(Tv(0), a); }

    // DC blocker cutoff
    void dcCutHz(Tv hz)   { dc.cutoffHz(hz); }

    void reset(){
        even.reset();
        dc.reset();
    }

    inline Tv operator()(Tv x, Tv g)
    {
        g = std::max(g, Tv(0));

        // dry reference (pre-GR)
        const Tv dry = x;

        // apply compressor gain reduction
        Tv y = x * g;

        // gain reduction amount in [0..1] for typical compression (g<=1)
        const Tv gr = scl::clip(Tv(1) - std::min(g, Tv(1)), Tv(0), Tv(1));

        // map GR -> even harmonic drive (safe curve)
        const Tv drive = (gr > Tv(0))
            ? std::exp(mEvenCurve * std::log(gr))   // pow(gr, evenCurve)
            : Tv(0);

        even.amount(mEvenMax * drive);

        // even-harmonic coloration
        y = even(y);

        // optional gentle clip (kept very smooth to avoid fizz)
        if(mClipAmt > Tv(0)){
            y = softClipNorm(y, mClipAmt);
        }

        // DC cleanup (asymmetry produces DC)
        y = dc(y);

        // wet/dry relative to original input (like analog “blend”)
        return (Tv(1) - mWet) * dry + mWet * y;
    }

    void onDomainChange(double r){
        even.onDomainChange(r);
        dc.onDomainChange(r);
    }

private:
    static inline Tv softClipNorm(Tv x, Tv amt){
        // amt=0 -> linear; larger -> more clip
        const Tv d = Tv(1) + amt;
        return std::tanh(d * x) / d;
    }

    Tv mWet      { Tv(1) };
    Tv mEvenMax  { Tv(0.6) };  // strong but safe default
    Tv mEvenCurve{ Tv(0.8) };  // ramps in musically
    Tv mClipAmt  { Tv(0.0) };  // off by default
};

} // namespace gam
