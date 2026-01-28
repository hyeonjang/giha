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

    template <typename Id, typename Key>
    PrimitiveOrbitParam(rhi::IDevice* device, const DartMap<Id, Key>& map) 
    : model(device, map) {
        Id cpuCounter = 0;
        orbitCounter = createBuffer(device, &cpuCounter, 1);
        offsetCounter = createBuffer(device, &cpuCounter, 1);

        std::vector<Id> zeroData(MaxLoop * map.count(), 0);
        indices = createBuffer(device, zeroData.data(), MaxLoop * map.count());
        values = createBuffer(device, zeroData.data(), MaxLoop * map.count());
        // indices = createBuffer(device, (Id*)nullptr, MaxLoop * model.cpuCounter);
        // values = createBuffer(device, (Id*)nullptr, MaxLoop * model.cpuCounter);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("orbitCounter").setBinding(orbitCounter);
        cursor.getPath("offsetCounter").setBinding(offsetCounter);
        cursor.getPath("indices").setBinding(indices);
        cursor.getPath("values").setBinding(values);
    }
};

using FaceOrbitParam = PrimitiveOrbitParam<3>;

} // namespace giha::slangrhi
#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG