// AnalogVCA.hpp
#pragma once
#include <algorithm>
#include <cmath>

template <class Sample>
class AnalogVCA {
public:
    enum class Model { Clean, THAT2181, DBX, SSL };

    void setModel(Model m) { model = m; }

    void setMakeupDb(Sample db) {
        makeup = std::pow((Sample)10, db / (Sample)20);
    }

    void setDryWet(Sample w) {
        wet = std::clamp(w, (Sample)0, (Sample)1);
    }

    // Imperfection controls
    void setSaturation(Sample s) { sat = std::max((Sample)0, s); }
    void setTHD(Sample d)        { thd = std::max((Sample)0, d); }
    void setBleed(Sample b)      { bleed = std::max((Sample)0, b); }

    inline Sample process(Sample x, Sample controlGain)
    {
        // --- control bleed ---
        Sample bleedSig = x * controlGain * bleed;

        // --- ideal VCA ---
        Sample y = x * controlGain * makeup;

        // --- model shaping ---
        switch (model)
        {
        case Model::THAT2181:
            y = saturate(y, sat * (Sample)0.8);
            y += thd * harmonic(y, 2);
            break;

        case Model::DBX:
            y = saturate(y, sat * (Sample)1.2);
            y += thd * (harmonic(y, 2) + (Sample)0.5 * harmonic(y, 3));
            break;

        case Model::SSL:
            y = saturate(y, sat * (Sample)0.6);
            y += thd * (harmonic(y, 3));
            break;

        default: break;
        }

        y += bleedSig;

        // parallel blend
        return ((Sample)1 - wet) * x + wet * y;
    }

private:
    static inline Sample saturate(Sample x, Sample a)
    {
        if (a <= 0) return x;
        return std::tanh(x * (Sample)(1 + a));
    }

    static inline Sample harmonic(Sample x, int order)
    {
        return std::pow(std::abs(x), order) * std::copysign((Sample)1, x);
    }

    Model  model { Model::Clean };
    Sample makeup { (Sample)1 };
    Sample wet    { (Sample)1 };
    Sample sat    { (Sample)0 };
    Sample thd    { (Sample)0 };
    Sample bleed  { (Sample)0 };
};

/*
This is where your compressor stops being “clean DSP” and starts sounding like **real hardware**.

We’ll add **three physical imperfections** to the VCA:

1. **Saturation** (non-linear transfer)
2. **THD** (harmonic distortion profile)
3. **Control bleed** (gain-dependent coloration)

All *after* the control signal but *inside* the gain stage — exactly how analog VCAs behave.

---

# 🧬 **Analog Personality Map**

| VCA      | Saturation | THD    | Bleed    | Sound          |
| -------- | ---------- | ------ | -------- | -------------- |
| THAT2181 | Medium     | Low    | Very Low | Clean, hi-fi   |
| DBX      | High       | Medium | Medium   | Punchy, gritty |
| SSL      | Low        | Medium | Low      | Tight, glue    |

---

# 🧠 Why This Works

These imperfections are **control-dependent**:
as gain changes, the distortion pattern changes → this is why analog compressors feel alive.

Your **GainController** already gives you a beautiful, smooth control voltage — this VCA converts that into **character**.

---

# 🧭 How to Integrate

In your main processor, replace:

```cpp
VCA vca;
```

with:

```cpp
AnalogVCA<float> vca;
```

and feed it the same control gain.

---

If you’d like, next we can:

* tune exact distortion curves for famous units (SSL G-Bus, DBX 160, API 2500),
* or add **control-voltage ripple & noise** for even deeper realism.
*/
