#pragma once

#ifdef GIHA_SLANG
#ifdef GIHA_SLANG_RHI

#include <slang-rhi.h>
#include <slang-rhi/shader-cursor.h>

#include <giha/geometry/dart.h>
#include <giha/slang/adapters/slang-rhi.hpp>

namespace giha::slangrhi {

//
// giha/geometry/dart.slang
//
struct SigmaModel {

    template <typename Id, typename Key>
    SigmaModel(rhi::IDevice* device, const giha::DartMap<Id, Key>& map, uint32_t padd = 0) {
        cpuCounter = static_cast<uint32_t>(map.count());
        counter = createBuffer(device, &cpuCounter, 1);
        vertNext = createBuffer(device, map.vNext.data(), map.vNext.size() + padd);
        edgeNext = createBuffer(device, map.eNext.data(), map.eNext.size() + padd);
        extrinsicKey = createBuffer(device, map.vKeys.data(), map.vKeys.size() + padd);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        cursor.getPath("counter").setBinding(counter);
        cursor.getPath("vertNext").setBinding(vertNext);
        cursor.getPath("edgeNext").setBinding(edgeNext);
        cursor.getPath("extrinsicKey").setBinding(extrinsicKey);
    }

public:
    uint32_t cpuCounter;
    Slang::ComPtr<rhi::IBuffer> counter;
    Slang::ComPtr<rhi::IBuffer> vertNext;
    Slang::ComPtr<rhi::IBuffer> edgeNext;
    Slang::ComPtr<rhi::IBuffer> extrinsicKey;
};

// 
// giha/geometry/geometry.slang
// 
struct ExtrinsicGeometry {

    template <typename T>
    ExtrinsicGeometry(rhi::IDevice* device, const SigmaModel& _model, T* posData, size_t posLength)
    : model(_model) {
        cpuCounter = posLength;
        counter = createBuffer(device, &cpuCounter, 1);
        vertexPositions = createBuffer(device, posData, sizeof(T)*posLength);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("counter").setBinding(counter);
        cursor.getPath("vertexPositions").setBinding(vertexPositions);
    }

public:
    SigmaModel model;
    uint32_t cpuCounter;
    Slang::ComPtr<rhi::IBuffer> counter;
    Slang::ComPtr<rhi::IBuffer> vertexPositions; // float3
};

struct IntrinsicGeometry {

    template <typename T>
    IntrinsicGeometry(rhi::IDevice* device, const SigmaModel& _model, T* data, size_t length)
    : model(_model) {
        dartLengths = createBuffer(device, data, sizeof(T)*length);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("dartLengths").setBinding(dartLengths);
    }

public:
    SigmaModel model;
    Slang::ComPtr<rhi::IBuffer> dartLengths;
};

} // namespace giha
#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG