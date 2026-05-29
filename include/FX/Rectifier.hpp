enum class RectifierType { HalfWave, FullWave, Square };

template <class Sample>
inline Sample rectify(Sample x, RectifierType type)
{
    switch (type)
    {
        case RectifierType::HalfWave: return std::max((Sample)0, x);
        case RectifierType::FullWave: return std::abs(x);
        case RectifierType::Square:   return x * x;
    }
    return (Sample)0;
}
