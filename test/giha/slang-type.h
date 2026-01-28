#pragma once

#include <slang-rhi/shader-cursor.h>
#include <giha/geometry/dart.h>
#include <cstdint>
#include <vector>

inline Slang::ComPtr<rhi::IBuffer> createBuffer(
    rhi::IDevice* device, void* data, size_t byteSize
) {
    return device->createBuffer(
        rhi::BufferDesc {
            .size = byteSize,
            .format = rhi::Format::Undefined,
            .memoryType = rhi::MemoryType::DeviceLocal,
            .usage = rhi::BufferUsage::ShaderResource | rhi::BufferUsage::UnorderedAccess | rhi::BufferUsage::CopySource | rhi::BufferUsage::CopyDestination,
            .defaultState = rhi::ResourceState::UnorderedAccess,
        }, data
    );
}

inline Slang::ComPtr<rhi::IBuffer> createRWBuffer(
    rhi::IDevice* device, void* data, size_t byteSize
) {
    return device->createBuffer(
        rhi::BufferDesc {
            .size = byteSize,
            .format = rhi::Format::Undefined,
            .memoryType = rhi::MemoryType::DeviceLocal,
            .usage = rhi::BufferUsage::UnorderedAccess | rhi::BufferUsage::CopySource | rhi::BufferUsage::CopyDestination,
            .defaultState = rhi::ResourceState::UnorderedAccess,
        }, data
    );
}

template <typename T>
Slang::ComPtr<rhi::IBuffer> createBuffer(rhi::IDevice* device, T* data, size_t length) { return createBuffer(device, (void*)data, length * sizeof(T)); }

template <typename T>
Slang::ComPtr<rhi::IBuffer> createRWBuffer(rhi::IDevice* device, T* data, size_t length) { return createRWBuffer(device, (void*)data, length * sizeof(T)); }

//
// giha/geometry/dart.slang
//
struct SigmaModel {

    template <typename Id, typename Key>
    SigmaModel(rhi::IDevice* device, const giha::DartMap<Id, Key>& map, uint32_t padd) {
        cpuCounter = static_cast<uint32_t>(map.count());
        counter = createRWBuffer(device, &cpuCounter, 1);
        vertNext = createRWBuffer(device, map.vNext.data(), map.vNext.size() + padd);
        edgeNext = createRWBuffer(device, map.eNext.data(), map.eNext.size() + padd);
        extrinsicKey = createRWBuffer(device, map.vKeys.data(), map.vKeys.size() + padd);
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

struct ExtrinsicGeometry {

    template <typename T>
    ExtrinsicGeometry(rhi::IDevice* device, const SigmaModel& _model, T* posData, size_t posLength)
    : model(_model) {
        cpuCounter = posLength;
        counter = createRWBuffer(device, &cpuCounter, 1);
        vertexPositions = createRWBuffer(device, posData, sizeof(T)*posLength);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("counter").setBinding(counter);
        cursor.getPath("vertexPositions").setBinding(vertexPositions);
    }

public:
    MutableSigmaModel model;
    uint32_t cpuCounter;
    Slang::ComPtr<rhi::IBuffer> counter;
    Slang::ComPtr<rhi::IBuffer> vertexPositions; // float3
};

struct IntrinsicGeometry {

    template <typename Id>
    IntrinsicGeometry(rhi::IDevice* device, const giha::DartMap<Id, uint32_t>& map, size_t size)
    : model(device, map) {
        dartLengths = createBuffer(device, nullptr, size);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("dartLengths").setBinding(dartLengths);
    }

public:
    SigmaModel model;
    Slang::ComPtr<rhi::IBuffer> dartLengths;
};

//
// giha_kernel/orbit.slang
//
struct PrimitiveOrbitParam {
    SigmaModel model;
    Slang::ComPtr<rhi::IBuffer> orbitCounter;
    Slang::ComPtr<rhi::IBuffer> offsetCounter;
    Slang::ComPtr<rhi::IBuffer> indices;
    Slang::ComPtr<rhi::IBuffer> values;

    PrimitiveOrbitParam() = default;
    PrimitiveOrbitParam(
        SigmaModel model_,
        Slang::ComPtr<rhi::IBuffer> orbitCounter_,
        Slang::ComPtr<rhi::IBuffer> offsetCounter_,
        Slang::ComPtr<rhi::IBuffer> indices_,
        Slang::ComPtr<rhi::IBuffer> values_
    )
    : model(std::move(model_))
    , orbitCounter(std::move(orbitCounter_))
    , offsetCounter(std::move(offsetCounter_))
    , indices(std::move(indices_))
    , values(std::move(values_)) {}

    void writeInto(rhi::ShaderCursor cursor) const {
        model.writeInto(cursor.getPath("model"));
        cursor.getPath("orbitCounter").setBinding(orbitCounter);
        cursor.getPath("offsetCounter").setBinding(offsetCounter);
        cursor.getPath("indices").setBinding(indices);
        cursor.getPath("values").setBinding(values);
    }
};
