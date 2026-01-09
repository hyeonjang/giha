#pragma once

#include <slang-rhi/shader-cursor.h>
#include <giha/geometry/dart.h>
#include <cstdint>
#include <vector>

inline Slang::ComPtr<rhi::IBuffer> createUniformBuffer(
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

template <typename T>
Slang::ComPtr<rhi::IBuffer> createUniformBuffer(
    rhi::IDevice* device, T* data, size_t length
) { return createUniformBuffer(device, (void*)data, length * sizeof(T)); }

struct ParamBlock {
public:
    using Writer = void(*)(const void*, rhi::ShaderCursor);
    
    void writeInto(rhi::ShaderCursor cursor) const {
        if (writer && instance)
            writer(instance, cursor);
    }

    template <typename T>
    static ParamBlock bind(const T* value) {
        return ParamBlock { value,
            [](const void* ptr, rhi::ShaderCursor cursor) {
                static_cast<const T*>(ptr)->writeInto(cursor);
            }
        };
    }

public:
    const void* instance = nullptr;
    Writer writer = nullptr;
};

//
// giha/geometry/dart.slang
//
struct DartBlock {

    template <typename Id, typename Key>
    DartBlock(rhi::IDevice* device, const giha::DartMap<Id, Key>& map) {
        vKeys = createUniformBuffer(device, map.vKeys.data(), map.vKeys.size());
        vNext = createUniformBuffer(device, map.vNext.data(), map.vNext.size());
        eNext = createUniformBuffer(device, map.eNext.data(), map.eNext.size());
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        cursor.getPath("vKeys").setBinding(vKeys);
        cursor.getPath("vNext").setBinding(vNext);
        cursor.getPath("eNext").setBinding(eNext);
    }

public:
    Slang::ComPtr<rhi::IBuffer> vKeys;
    Slang::ComPtr<rhi::IBuffer> vNext;
    Slang::ComPtr<rhi::IBuffer> eNext;
};

// 
// giha/hash.slang
// 
template <typename Key>
struct HashSet {

    HashSet(rhi::IDevice* device, size_t slotCount = 100000) {
        std::vector<int> initial(slotCount, -1);
        buffer = createUniformBuffer(device, initial.data(), slotCount);
    }

    void writeInto(rhi::ShaderCursor cursor) const {
        cursor.getPath("buffer").setBinding(buffer);
    }

private:
    Slang::ComPtr<rhi::IBuffer> buffer;
};