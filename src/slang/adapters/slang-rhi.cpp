#include <giha/slang/adapters/slang-rhi.hpp>

#ifdef GIHA_SLANG
#ifdef GIHA_SLANG_RHI

namespace giha::slangrhi {

ComputeKernel::ComputeKernel() = default;

ComputeKernel::ComputeKernel(rhi::IDevice* device, slang::IComponentType* kernel) {
    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    program = device->createShaderProgram(kernel, diagnosticBlob.writeRef());
    CHECK(program != nullptr, diagonize(diagnosticBlob).c_str());

    rhi::ComputePipelineDesc desc = {};
    desc.program = program.get();
    pipeline = device->createComputePipeline(desc);
}

std::vector<ComputeKernel> makeComputeKernels(
    rhi::IDevice* device,
    std::vector<Slang::ComPtr<slang::IComponentType>> kernels
) {
    std::vector<ComputeKernel> result;
    result.reserve(kernels.size());
    for (auto kernel : kernels) {
        result.emplace_back(device, kernel);
    }
    return result;
}

} // namespace giha::slangrhi

#endif // GIHA_SLANG_RHI
#endif // GIHA_SLANG
