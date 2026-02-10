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
        idSize = sizeof(Id);
        keySize = sizeof(Key);

        vertNext = createBuffer(device, map.vNext.data(), map.vNext.size() + padd);
        edgeNext = createBuffer(device, map.eNext.data(), map.eNext.size() + padd);
        inducedVertexMap = createBuffer(device, map.vKeys.data(), map.vKeys.size() + padd);

        cpuCounter = static_cast<uint32_t>(map.count());
        counter = createBuffer(device, &cpuCounter, 1);
        ownershipFlags = createBuffer(device, nullptr, sizeof(i32) * (map.count() + padd));
    }

    void writeInto(rhi::ShaderCursor cursor) const {

        {
            auto vertCursor = cursor.getPath("vertNext");
            CHECK(vertCursor.isValid(), "Missing SigmaModel.vertNext");
            CHECK_SLANG(vertCursor.setBinding(vertNext), "Failed to bind vertNext");

            auto edgeCursor = cursor.getPath("edgeNext");
            CHECK(edgeCursor.isValid(), "Missing SigmaModel.edgeNext");
            CHECK_SLANG(edgeCursor.setBinding(edgeNext), "Failed to bind edgeNext");

            auto keyCursor = cursor.getPath("inducedVertexMap");
            CHECK(keyCursor.isValid(), "Missing SigmaModel.inducedVertexMap");
            CHECK_SLANG(keyCursor.setBinding(inducedVertexMap), "Failed to bind inducedVertexMap");
        }

        // for mutable        
        auto modelCursor = cursor.getPath("model");
        if (modelCursor.isValid()) {

            auto vertCursor = modelCursor.getPath("vertNext");
            CHECK(vertCursor.isValid(), "Missing SigmaModel.vertNext");
            CHECK_SLANG(vertCursor.setBinding(vertNext), "Failed to bind vertNext");

            auto edgeCursor = modelCursor.getPath("edgeNext");
            CHECK(edgeCursor.isValid(), "Missing SigmaModel.edgeNext");
            CHECK_SLANG(edgeCursor.setBinding(edgeNext), "Failed to bind edgeNext");

            auto keyCursor = modelCursor.getPath("inducedVertexMap");
            CHECK(keyCursor.isValid(), "Missing SigmaModel.inducedVertexMap");
            CHECK_SLANG(keyCursor.setBinding(inducedVertexMap), "Failed to bind inducedVertexMap");

            auto counterCursor = cursor.getPath("counter");
            if (counterCursor.isValid()) {
                CHECK_SLANG(counterCursor.setBinding(counter), "Failed to bind counter");
            }

            auto vertNextAtomicCursor = cursor.getPath("vertNextAtomic");
            auto edgeNextAtomicCursor = cursor.getPath("edgeNextAtomic");
            if (vertNextAtomicCursor.isValid() && edgeNextAtomicCursor.isValid()) {
                CHECK_SLANG(vertNextAtomicCursor.setBinding(vertNext), "Failed to bind vertNextAtomic");
                CHECK_SLANG(edgeNextAtomicCursor.setBinding(edgeNext), "Failed to bind edgeNextAtomic");
            }

            auto ownershipFlagsCursor = cursor.getPath("ownershipFlags");
            if (ownershipFlagsCursor.isValid()) {
                CHECK_SLANG(ownershipFlagsCursor.setBinding(ownershipFlags), "Failed to bind ownershipFlags");
            }
        }
    }

    size_t idSize, keySize;

    Slang::ComPtr<rhi::IBuffer> vertNext;
    Slang::ComPtr<rhi::IBuffer> edgeNext;
    Slang::ComPtr<rhi::IBuffer> inducedVertexMap;

    uint32_t cpuCounter;
    Slang::ComPtr<rhi::IBuffer> counter;
    Slang::ComPtr<rhi::IBuffer> ownershipFlags; // int32
};

// 
// giha/geometry/geometry.slang
// 
struct ExtrinsicGeometry {

    template <typename T>
    ExtrinsicGeometry(rhi::IDevice* device, const SigmaModel& _model, T* posData, size_t posLength, size_t padd = 0)
    : model(_model) {
        cpuCounter = posLength;
        counter = createBuffer(device, &cpuCounter, 1);
        vertexPositions = createBuffer(device, posData, sizeof(T) * (posLength + padd));
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        auto modelCursor = cursor.getPath("model");
        CHECK(modelCursor.isValid(), "Missing ExtrinsicGeometry.model");
        model.writeInto(modelCursor);

        auto counterCursor = cursor.getPath("counter");
        CHECK(counterCursor.isValid(), "Missing ExtrinsicGeometry.counter");
        CHECK_SLANG(counterCursor.setBinding(counter), "Failed to bind counter");

        auto positionsCursor = cursor.getPath("vertexPositions");
        CHECK(positionsCursor.isValid(), "Missing ExtrinsicGeometry.vertexPositions");
        CHECK_SLANG(positionsCursor.setBinding(vertexPositions), "Failed to bind vertex positions");
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
        auto modelCursor = cursor.getPath("model");
        CHECK(modelCursor.isValid(), "Missing IntrinsicGeometry.model");
        model.writeInto(modelCursor);

        auto dartCursor = cursor.getPath("dartLengths");
        CHECK(dartCursor.isValid(), "Missing IntrinsicGeometry.dartLengths");
        CHECK_SLANG(dartCursor.setBinding(dartLengths), "Failed to bind dart lengths");
    }

public:
    SigmaModel model;
    Slang::ComPtr<rhi::IBuffer> dartLengths;
};

} // namespace giha
#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG
