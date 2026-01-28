#pragma once

#ifdef GIHA_SLANG
#ifdef GIHA_SLANG_RHI

#include <giha/slang/runtime.hpp>

#include <slang-com-ptr.h>
#include <slang-rhi.h>

#include <initializer_list>
#include <vector>

namespace giha::slangrhi {

class ComputeKernel {
public:
    ComputeKernel();
    ComputeKernel(rhi::IDevice* device, slang::IComponentType* kernel);

    Slang::ComPtr<rhi::IShaderProgram> program;
    Slang::ComPtr<rhi::IComputePipeline> pipeline;
};

std::vector<ComputeKernel> makeComputeKernels(
    rhi::IDevice* device,
    std::vector<Slang::ComPtr<slang::IComponentType>> kernels
);

template <typename T>
std::vector<T> readBuffer(rhi::IDevice* device, rhi::IBuffer* buffer, size_t length) {
    Slang::ComPtr<ISlangBlob> blob;
    CHECK_SLANG(
        device->readBuffer(buffer, 0, length * sizeof(T), blob.writeRef()),
        "Failed to read back buffer"
    );

    const size_t gpuElementCount = blob->getBufferSize() / sizeof(T);
    const T* deviceData = reinterpret_cast<const T*>(blob->getBufferPointer());
    return std::vector<T>(deviceData, deviceData + gpuElementCount);
}

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

template <typename T>
Slang::ComPtr<rhi::IBuffer> createBuffer(rhi::IDevice* device, T* data, size_t length) { 
    return createBuffer(device, (void*)data, length * sizeof(T)); 
}
} // namespace giha::slangrhi

#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG
