#ifndef ANDROID_FXLAB_DOUBLINGDESCRIPTION_H
#define ANDROID_FXLAB_DOUBLINGDESCRIPTION_H

#include "EffectDescription.h"
#include "../DoublingEffect.h"

namespace Effect {
class DoublingDescription: public EffectDescription<DoublingDescription, 3> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("Doubling");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("Delay");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array<ParamType, getNumParams()> {
            ParamType("Depth (ms)", 10, 100, 40),
            ParamType("Delay (ms)", 1, 100, 40),
            ParamType("Noise pass", 1, 10, 4),
        };
    }
    template<class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        return _ef<iter_type> {
            DoublingEffect<iter_type>{paramArr[0], paramArr[1], paramArr[2]}
        };
    }
};
} //namespace Effect
#endif //ANDROID_FXLAB_DOUBLINGDESCRIPTION_H
