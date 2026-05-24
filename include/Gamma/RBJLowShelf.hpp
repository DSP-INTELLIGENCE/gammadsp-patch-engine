#pragma once
#include <cmath>
#include <algorithm>

#include "Gamma/Domain.h"
#include "Gamma/Types.h"

namespace gam {

template<class T = gam::real, class Td = GAM_DEFAULT_DOMAIN>
class RBJLowShelf : public Td {
public:
    RBJLowShelf(T freqHz=T(200), T slope=T(1), T gainDb=T(0)){
        set(freqHz,slope,gainDb);
        reset();
    }

    void reset(T v=T(0)){ s1=s2=v; }

    void freq(T f){ mFreq=clampFreq(f); compute(); }
    void slope(T s){ mSlope=std::max(s,T(1e-6)); compute(); }
    void gainDb(T g){ mGainDb=g; compute(); }

    void set(T f,T s,T g){
        mFreq=clampFreq(f);
        mSlope=std::max(s,T(1e-6));
        mGainDb=g;
        compute();
    }

    inline T operator()(T x){
        const T y=b0*x+s1;
        s1=b1*x-a1*y+s2;
        s2=b2*x-a2*y;
        return y;
    }

    void onDomainChange(double){ compute(); }

private:
    T mFreq=T(200), mSlope=T(1), mGainDb=T(0);
    T b0,b1,b2,a1,a2;
    T s1=0,s2=0;

    inline T clampFreq(T f) const {
        const T fs=T(Td::spu());
        return std::clamp(f,T(1e-4),fs*T(0.499));
    }

    void compute(){
        const T fs=T(Td::spu());
        if(fs<=0) return;

        const T A=std::pow(T(10),mGainDb/T(40));
        const T w0=T(2)*T(M_PI)*mFreq/fs;
        const T cw=std::cos(w0);
        const T sw=std::sin(w0);

        const T beta=std::sqrt(A)/mSlope;
        const T twoSqrtAalpha=sw*beta;

        const T bb0= A*((A+1)-(A-1)*cw+twoSqrtAalpha);
        const T bb1=2*A*((A-1)-(A+1)*cw);
        const T bb2= A*((A+1)-(A-1)*cw-twoSqrtAalpha);
        const T aa0=    (A+1)+(A-1)*cw+twoSqrtAalpha;
        const T aa1=-2*((A-1)+(A+1)*cw);
        const T aa2=    (A+1)+(A-1)*cw-twoSqrtAalpha;

        const T invA0=T(1)/aa0;
        b0=bb0*invA0; b1=bb1*invA0; b2=bb2*invA0;
        a1=aa1*invA0; a2=aa2*invA0;
    }
};

} // namespace gam
