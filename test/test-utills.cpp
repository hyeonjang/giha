#include "suite-test.hpp"

#include <random>

#include <slang-rhi.h>
#include <slang-rhi/shader-cursor.h>
#include <giha/slang/slang.h>
#include <giha/slang/adapters/slang-rhi.hpp>

DECLARE_SUITE(UtillsSuite);
using namespace giha;
using namespace giha::slangrhi;

extern Slang::ComPtr<rhi::IDevice> gDevice;
static giha::SlangKernelCache gKernelCache;

TEST_IN(UtillsSuite, UtillPrepare) {

    // be ready session
    gKernelCache.reset(gDevice->getSlangSession());
}

TEST_IN(UtillsSuite, RadixSort) {

    constexpr size_t n = 2048;

    std::mt19937 gen(0);
    std::uniform_int_distribution<int> dist(0, n);

    std::vector<u32> elements;
    elements.reserve(n);
    for (int i=0; i<n; i++) elements.push_back(dist(gen));

    u32 gNumBatchPerWorkGroup = 1;
    u32 elementsPerGroup = WORKGROUP_SIZE * gNumBatchPerWorkGroup;
    u32 groupCount = (elements.size() + elementsPerGroup - 1) / elementsPerGroup;

    auto elementsIn = createBuffer(gDevice, elements.data(), elements.size());
    auto histogram = createBuffer(gDevice, nullptr, groupCount * 256 * sizeof(u32));
    auto elementsOut = createBuffer(gDevice, nullptr, elements.size() * sizeof(u32));

    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "BuildHistogram", { "uint", "U32RadixDigitSource" } } } },
        { "module/giha_kernel.slang", { { "SortByScatter",  { "uint", "U32RadixDigitSource" } } } }
    }));

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    // 
    auto computePass = encoder->beginComputePass();
    for (int i = 0; i < 4; i++) {

        // run build histogram        
        {
            auto buildHistogram = computePass->bindPipeline(kernels[0].pipeline);

            rhi::ShaderCursor(buildHistogram).getPath("gNumBatchPerWorkGroup").setData(&gNumBatchPerWorkGroup, sizeof(u32));

            auto entryPointCursor = rhi::ShaderCursor(buildHistogram->getEntryPoint(0));
            CHECK_SLANG(entryPointCursor.getPath("elements").setBinding(elementsIn), "");
            CHECK_SLANG(entryPointCursor.getPath("histogram").setBinding(histogram), "");

            u32 passIndex = i;
            CHECK_SLANG(entryPointCursor.getPath("passIndex").setData(&passIndex, sizeof(u32)), "");

            computePass->dispatchCompute(elements.size(), 1, 1);
        }

        // run sorting
        {
            auto sortByScatter = computePass->bindPipeline(kernels[1].pipeline);

            rhi::ShaderCursor(sortByScatter).getPath("gNumBatchPerWorkGroup").setData(&gNumBatchPerWorkGroup, sizeof(u32));

            auto entryPointCursor = rhi::ShaderCursor(sortByScatter->getEntryPoint(0));
            CHECK_SLANG(entryPointCursor.getPath("elements").setBinding(elementsIn), "");
            CHECK_SLANG(entryPointCursor.getPath("histogram").setBinding(histogram), "");

            u32 passIndex = i;
            CHECK_SLANG(entryPointCursor.getPath("passIndex").setData(&passIndex, sizeof(u32)), "");
            CHECK_SLANG(entryPointCursor.getPath("elementsOut").setBinding(elementsOut), "");

            computePass->dispatchCompute(elements.size(), 1, 1);
        }

        encoder->copyBuffer(elementsIn, 0, elementsOut, 0, sizeof(u32) * elements.size());
    }
    computePass->end();
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer");

    
    // sort test
    std::vector<u32> gpuResult = slangrhi::readBuffer<u32>(gDevice, elementsOut, elements.size());

    std::vector<u32> expected = elements;
    std::sort(expected.begin(), expected.end());

    REQUIRE_ARRAY_EQ(expected.data(), gpuResult.data(), gpuResult.size());
}
