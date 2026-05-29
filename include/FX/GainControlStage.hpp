#pragma once
#include <algorithm>
#include <cmath>

#include "DigitalGainStage.hpp"
#include "AnalogVCA.hpp"
#include "OpticalGainCell.hpp"
#include "FETGainStage.hpp"
#include "VariableMuTubeStage.hpp"

template <class Sample>
class GainControlStage {
public:
    enum class Type { Digital, VCA, Optical, FET, VariableMu };

    void setType(Type t) { type = t; }

    void setMakeupDb(Sample db)
    {
        digital.setMakeupDb(db);
        vca.setMakeupDb(db);
        optical.setMakeupDb(db);
        fet.setMakeupDb(db);
        tube.setMakeupDb(db);
    }

    void setDryWet(Sample w)
    {
        digital.setDryWet(w);
        vca.setDryWet(w);
        optical.setDryWet(w);
        fet.setDryWet(w);
        tube.setDryWet(w);
    }

    void reset()
    {
        digital.reset();
        vca.reset();
        optical.reset();
        fet.reset();
        tube.reset();
    }

    inline Sample process(Sample input, Sample controlGain)
    {
        switch (type)
        {
            case Type::Digital:   return digital.process(input, controlGain);
            case Type::VCA:       return vca.process(input, controlGain);
            case Type::Optical:   return optical.process(input, controlGain);
            case Type::FET:       return fet.process(input, controlGain);
            case Type::VariableMu:return tube.process(input, controlGain);
        }
        return input;
    }

    // Access to character parameters
    AnalogVCA<Sample>&       vcaModel()     { return vca; }
    OpticalGainCell<Sample>& opticalModel() { return optical; }
    FETGainStage<Sample>&    fetModel()     { return fet; }
    VariableMuTubeStage<Sample>& tubeModel(){ return tube; }

private:
    Type type { Type::Digital };

    DigitalGainStage<Sample>  digital;
    AnalogVCA<Sample>        vca;
    OpticalGainCell<Sample>  optical;
    FETGainStage<Sample>     fet;
    VariableMuTubeStage<Sample> tube;
};
