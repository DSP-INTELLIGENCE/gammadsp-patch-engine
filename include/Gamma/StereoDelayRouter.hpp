#pragma once
#include "Gamma/Domain.h"
#include "Gamma/Types.h"
#include "Gamma/scl.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class StereoDelayRouter : public Td {
public:
    StereoDelayRouter(){
        reset();
        stereo(); // sensible default
    }

    void reset(){
        // Identity routing
        ll = rr = T(1);
        lr = rl = T(0);
        monoGain = T(1);
        width = T(1);
    }

    // --------------------------------------------------
    // Presets
    // --------------------------------------------------

    /// L->L, R->R
    void stereo(){
        ll = rr = T(1);
        lr = rl = T(0);
    }

    /// Mono input equally to L/R
    void mono(){
        ll = rr = T(0.5);
        lr = rl = T(0.5);
    }

    /// Ping-pong feedback style
    void pingPong(){
        ll = rr = T(0);
        lr = rl = T(1);
    }

    /// Haas-style crossfeed
    void cross(T amt){
        amt = scl::clip(amt, T(1), T(0));
        ll = rr = T(1) - amt;
        lr = rl = amt;
    }

    // --------------------------------------------------
    // Parameters
    // --------------------------------------------------

    /// Direct matrix control
    void matrix(T LL, T LR, T RL, T RR){
        ll = LL; lr = LR;
        rl = RL; rr = RR;
    }

    /// Mono → stereo spread (0 = mono, 1 = full stereo)
    void stereoWidth(T w){
        width = scl::clip(w, T(2), T(0));
    }

    /// Gain applied to mono input before routing
    void monoInputGain(T g){
        monoGain = g;
    }

    // --------------------------------------------------
    // Processing
    // --------------------------------------------------

    /// Mono input → stereo output
    inline Vec<2,T> operator()(T x){
        x *= monoGain;

        T l = ll * x + rl * x;
        T r = lr * x + rr * x;

        applyWidth(l, r);
        return Vec<2,T>(l, r);
    }

    /// Stereo input → stereo output
    inline Vec<2,T> operator()(T inL, T inR){
        T l = ll * inL + rl * inR;
        T r = lr * inL + rr * inR;

        applyWidth(l, r);
        return Vec<2,T>(l, r);
    }

    /// Stereo vector overload
    inline Vec<2,T> operator()(const Vec<2,T>& v){
        return (*this)(v[0], v[1]);
    }

private:
    // Routing matrix
    T ll = T(1), lr = T(0);
    T rl = T(0), rr = T(1);

    // Controls
    T monoGain = T(1);
    T width    = T(1);

    // Width control via mid/side
    inline void applyWidth(T& l, T& r){
        if(width == T(1)) return;

        T mid  = (l + r) * T(0.5);
        T side = (l - r) * T(0.5) * width;

        l = mid + side;
        r = mid - side;
    }
};

} // namespace gam
