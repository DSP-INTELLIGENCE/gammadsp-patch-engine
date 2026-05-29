#pragma once
#include "LUTTransferFunction.hpp"

class LUTWaveShaper : public Function
{
public:
    LUTWaveShaper()
    : transfer(LUTTransferFunction::TransferType::SOFTCLIP)
    {}

    void setDrive(float v)   { drive   = std::max(0.0f, v); }
    void setOutput(float v)  { outGain = std::max(0.0f, v); }
    void setBias(float v)    { bias    = v; }
    void setAsym(float v)    { asym    = std::max(0.0f, v); }
    void setDiodeDrive(float v) { transfer.setDrive(v); }
    void setDiodeAsym(float p, float n) { transfer.setAsym(p, n); }
    void setDiodeThreshold(float t) { transfer.setThreshold(t); }
    void setTube(float a, float b, float k) {
        transfer.setTubeABC(a, b, k);
    }

    void setTubeAsym(float kp, float kn) {
        transfer.setTubeAsym(kp, kn);
    }

    void setType(LUTTransferFunction::TransferType t)
    {
        transfer.setType(t);
    }

    inline float process(float x) override
    {
        // Pre-shaping
        x = x * drive + bias;

        // Asymmetry
        if (x >= 0.0f) 
            x *= asym;

        // Nonlinearity
        x = transfer.process(x);
        x = dcblock.process(x);
        // Output trim
        return x * outGain;
    }

    LUTTransferFunction& getTransfer() { return transfer; }

    inline float curveColor(float x)
    {
        switch (transfer.currentType)
        {
            case LUTTransferFunction::TransferType::HARDCLIP:
                return x * 0.90f;

            case LUTTransferFunction::TransferType::TUBE_TRIODE:
                return x * 1.10f;

            case LUTTransferFunction::TransferType::DIODE_ASYM:
                return x * 1.05f;

            case LUTTransferFunction::TransferType::SOFTCLIP:
                return x * 1.02f;

            default:
                return x;
        }
    }

private:
    float drive   = 1.0f;
    float outGain = 1.0f;
    float bias    = 0.0f;
    float asym    = 1.0f;

    LUTTransferFunction transfer;
    BlockDC dcblock;
};
