#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class SidechainInput {
public:
    enum class Source { Internal, External, Blend };
    enum class Mode   { StereoSum, Left, Right, Mid, Side };

    void setSource(Source s) { source = s; }
    void setMode(Mode m)     { mode = m; }
    void setBlend(Sample b)  { blend = std::clamp(b, (Sample)0, (Sample)1); }

    void setFilterEnabled(bool b) { useFilter = b; }

    // User supplies any filter with: Sample process(Sample)
    template <class Filter>
    void setFilter(Filter* f) { filter = (void*)f; processFilter = &callFilter<Filter>; }

    inline Sample process(Sample inL, Sample inR,
                          Sample extL, Sample extR)
    {
        // --- select source ---
        Sample l = 0, r = 0;
        if (source == Source::Internal) { l = inL; r = inR; }
        else if (source == Source::External) { l = extL; r = extR; }
        else {
            l = (Sample)1 - blend * inL + blend * extL;
            r = (Sample)1 - blend * inR + blend * extR;
        }

        // --- channel mode ---
        Sample sc = 0;
        switch (mode)
        {
            case Mode::Left:  sc = l; break;
            case Mode::Right: sc = r; break;
            case Mode::Mid:   sc = (l + r) * (Sample)0.5; break;
            case Mode::Side:  sc = (l - r) * (Sample)0.5; break;
            default:          sc = (std::abs(l) > std::abs(r)) ? l : r; break;
        }

        // --- filter ---
        if (useFilter && filter) sc = processFilter(filter, sc);

        return sc;
    }

private:
    template <class Filter>
    static Sample callFilter(void* f, Sample x)
    {
        return static_cast<Filter*>(f)->process(x);
    }

    Source source { Source::Internal };
    Mode   mode   { Mode::StereoSum };
    Sample blend  { 0 };

    bool useFilter { false };

    void* filter { nullptr };
    Sample (*processFilter)(void*, Sample) { nullptr };
};
