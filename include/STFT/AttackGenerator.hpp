// AttackGenerator.hpp
#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class AttackGenerator {
public:
    enum class Shape { Exponential, Linear, SCurve };

    AttackGenerator()
    {
        setTime((Sample)0.01);
        setShape(Shape::Exponential);
    }

    void setTime(Sample seconds)
    {
        mTime = std::max((Sample)1e-6, seconds);
        updateCoeff();
    }

    void setShape(Shape s) { mShape = s; }

    void setProgramDependence(Sample amount)
    {
        // 0 = fixed, 1 = fully program dependent
        mProgram = std::clamp(amount, (Sample)0, (Sample)1);
    }

    void reset(Sample v = 0) { mValue = v; }

    Sample process(Sample input)
    {
        Sample delta = input - mValue;
        if (delta <= 0) return mValue;

        Sample coeff = mCoeff;

        // Program dependent scaling
        Sample scale = (Sample)1 + mProgram * std::abs(delta);
        coeff = std::pow(coeff, scale);

        switch (mShape)
        {
            case Shape::Linear:
                mValue += (1 - coeff) * delta;
                break;

            case Shape::SCurve:
            {
                Sample t = (Sample)1 - coeff;
                Sample s = t * t * ((Sample)3 - (Sample)2 * t); // smoothstep
                mValue += s * delta;
                break;
            }

            case Shape::Exponential:
            default:
                mValue = (1 - coeff) * input + coeff * mValue;
                break;
        }

        return mValue;
    }

    Sample value() const { return mValue; }

private:
    void updateCoeff()
    {
        mCoeff = std::exp((Sample)-1 / (mTime * gam::sampleRate()));
    }

    Sample mTime { (Sample)0.01 };
    Sample mCoeff { (Sample)0.0 };
    Sample mValue { (Sample)0.0 };
    Sample mProgram { (Sample)0.0 };
    Shape  mShape { Shape::Exponential };
};
