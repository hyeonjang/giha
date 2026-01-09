#include "suite-test.hpp"
#include "giha/slang-util.hpp"

#include <slang-rhi.h>
#include <giha/slang/slang.h>
#include <random>

DECLARE_SUITE(UtilsSuite);
using namespace giha;

extern Slang::ComPtr<rhi::IDevice> gDevice;
static giha::SlangSession gSession;

TEST_IN(UtilsSuite, Prepare) {

    // be ready session
    gSession = giha::SlangSession(gDevice->getSlangSession());
}

TEST_IN(UtilsSuite, RadixSort) {

    constexpr size_t n = 256;

    std::mt19937 gen(0);
    std::uniform_int_distribution<int> dist(0, n);

    std::vector<u32> elements;
    elements.reserve(n);
    for (int i=0; i<n; i++) elements.push_back(dist(gen));

    u32 gNumBatchPerWorkGroup = 1;
    u32 elementsPerGroup = WORKGROUP_SIZE * gNumBatchPerWorkGroup;
    u32 groupCount = (elements.size() + elementsPerGroup - 1) / elementsPerGroup;

    auto bElements = createUniformBuffer(gDevice, elements.data(), elements.size());
    auto histogram = createUniformBuffer(gDevice, nullptr, groupCount * 256 * sizeof(u32));
    auto elementsOut = createUniformBuffer(gDevice, nullptr, elements.size() * sizeof(u32));

    auto kernels = slangrhi::makeComputeKernels(gDevice, gSession, {
        { "module/sort.cs.slang", { { "BuildHistogram", { "uint", "UIntRadixDigitSource" } } } },
        { "module/sort.cs.slang", { { "SortByScatter", { "uint", "UIntRadixDigitSource" } } } }
    });

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    // 
    auto computePass = encoder->beginComputePass();
    for (int i = 0; i < 4; i++) {

        
        // run build histogram        
        constexpr u32 gNumBatchPerWorkGroup = 1; 
        {
            auto buildHistogram = computePass->bindPipeline(kernels[0].pipeline);

            rhi::ShaderCursor(buildHistogram).getPath("gNumBatchPerWorkGroup").setData(&gNumBatchPerWorkGroup, sizeof(u32));

            auto entryPointCursor = rhi::ShaderCursor(buildHistogram->getEntryPoint(0));
            CHECK_SLANG(entryPointCursor.getPath("elements").setBinding(bElements), "");
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
            CHECK_SLANG(entryPointCursor.getPath("elements").setBinding(bElements), "");
            CHECK_SLANG(entryPointCursor.getPath("histogram").setBinding(histogram), "");

            u32 passIndex = i;
            CHECK_SLANG(entryPointCursor.getPath("passIndex").setData(&passIndex, sizeof(u32)), "");
            CHECK_SLANG(entryPointCursor.getPath("elementsOut").setBinding(elementsOut), "");

            computePass->dispatchCompute(elements.size(), 1, 1);
        }

        encoder->copyBuffer(bElements, 0, elementsOut, 0, sizeof(u32) * elements.size());
    }
    computePass->end();
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer");


    Slang::ComPtr<ISlangBlob> blob;
    CHECK_SLANG(gDevice->readBuffer(elementsOut, 0, elements.size() * sizeof(u32), blob.writeRef()), "Faild to read back buffer");

    const size_t gpuElementCount = blob->getBufferSize() / sizeof(u32);
    REQUIRE(gpuElementCount == elements.size());

    std::vector<u32> expected = elements;
    std::sort(expected.begin(), expected.end());

    const u32* deviceData = reinterpret_cast<const u32*>(blob->getBufferPointer());
    std::vector<u32> gpuResult(deviceData, deviceData + gpuElementCount);

    REQUIRE_ARRAY_EQ(expected.data(), gpuResult.data(), gpuElementCount);
}
