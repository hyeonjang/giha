#pragma once

#ifdef GIHA_SLANG
#ifdef GIHA_SLANG_RHI

#include <giha/slang/adapters/slang-rhi.hpp>
#include <giha/geometry/adapters/slang-rhi/geometry.h>

namespace giha::slangrhi {
//
// giha_kernel/orbit.slang
//
template <uint32_t maxLoop = 3>
struct PrimitiveOrbitParam {

    constexpr static uint32_t MaxLoop = maxLoop;

    SigmaModel model;
    Slang::ComPtr<rhi::IBuffer> orbitCounter;
    Slang::ComPtr<rhi::IBuffer> offsetCounter;
    Slang::ComPtr<rhi::IBuffer> indices;
    Slang::ComPtr<rhi::IBuffer> values;

    PrimitiveOrbitParam() = default;
    PrimitiveOrbitParam(rhi::IDevice* device, const SigmaModel& model)
    : model(model) {
        uint32_t zero = 0;
        orbitCounter = createBuffer(device, &zero, sizeof(uint32_t));
        offsetCounter = createBuffer(device, &zero, sizeof(uint32_t));
        indices = createBuffer(device, nullptr, sizeof(uint32_t) * MaxLoop * model.cpuCounter);
        values = createBuffer(device,  nullptr, sizeof(model.keySize) * MaxLoop * model.cpuCounter);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        auto modelCursor = cursor.getPath("model");
        CHECK(modelCursor.isValid(), "Missing PrimitiveOrbitParam.model");
        model.writeInto(modelCursor);

        auto orbitCursor = cursor.getPath("orbitCounter");
        CHECK(orbitCursor.isValid(), "Missing PrimitiveOrbitParam.orbitCounter");
        CHECK_SLANG(orbitCursor.setBinding(orbitCounter), "Failed to bind orbitCounter");

        auto offsetCursor = cursor.getPath("offsetCounter");
        CHECK(offsetCursor.isValid(), "Missing PrimitiveOrbitParam.offsetCounter");
        CHECK_SLANG(offsetCursor.setBinding(offsetCounter), "Failed to bind offsetCounter");

        auto indicesCursor = cursor.getPath("indices");
        CHECK(indicesCursor.isValid(), "Missing PrimitiveOrbitParam.indices");
        CHECK_SLANG(indicesCursor.setBinding(indices), "Failed to bind indices");

        auto valuesCursor = cursor.getPath("values");
        CHECK(valuesCursor.isValid(), "Missing PrimitiveOrbitParam.values");
        CHECK_SLANG(valuesCursor.setBinding(values), "Failed to bind values");
    }
};

using FaceOrbitParam = PrimitiveOrbitParam<3>;

} // namespace giha::slangrhi
#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG
