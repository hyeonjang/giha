#include "suite-test.hpp"

#include <slang-rhi.h>

#include <giha/slang/slang.h>
#include <giha/slang/adapters/slang-rhi.hpp>
#include <giha/geometry/dart.h>
#include <giha/geometry/happly.h>
#include <giha/geometry/polygonmesh.h>

#include "giha/geometry-assets.h"
#include "giha/slang-type.h"

DECLARE_SUITE(GeometrySuite);
using namespace giha;

extern Slang::ComPtr<rhi::IDevice> gDevice;
static giha::SlangKernelCache gKernelCache;
extern const std::string gTestPath;

// 0. prepare slang session && @@todo loading test model
TEST_IN(GeometrySuite, PrepareGeometrySession) {
    gKernelCache.reset(gDevice->getSlangSession());
}

// 1. dart map (sigma model)
TEST_IN(GeometrySuite, BuildDartMap) {
    const auto& map = gGeometryAssets.dartMap<f64, u32>(gTestPath + "/resource/octahedron.ply");

    REQUIRE(map.count() == 24);

    auto countVertexDegree = [&](u32 dartID) -> u32 {
        u32 count = 0;
        u32 startDartID = dartID;
        do {
            count++;
            dartID = map.vNext[dartID];
        } while (dartID != startDartID);
        return count;
    };

    // 1. check dart count per vertex
    for (u32 dartID = 0; dartID < map.count(); ++dartID) {
        u32 degree = countVertexDegree(dartID);

        REQUIRE(degree == 4);
    }

    auto checkEdgeTwin = [&](u32 dartID) -> bool {
        u32 twinTwin = map.eNext[map.eNext[dartID]];
        return twinTwin == dartID;
    };

    // 2. check edge consistency: input manifold assumption
    for (u32 dartID = 0; dartID < map.count(); ++dartID) {
        REQUIRE(checkEdgeTwin(dartID));
    }
}

// 2. triangulation via face orbits (output for rendering engine)
TEST_IN(GeometrySuite, Triangulate) {

    // cpu prepare
    const std::string path = gTestPath + "/resource/octahedron.ply";
    const auto& polymesh = gGeometryAssets.mesh<f64, u32>(path);
    const auto& map = gGeometryAssets.dartMap<f64, u32>(path);

    using VertexIdType = u32;

    // prepare uniform buffer
    SigmaModel dart(gDevice, map);
    u32 zero = 0;
    auto orbitCounter = createBuffer(gDevice, &zero, 1);
    auto offsetCounter = createBuffer(gDevice, &zero, 1);
    const size_t maxEntries = 3 * map.count();
    std::vector<VertexIdType> zeroBuffer(maxEntries, 0);
    auto indices = createBuffer(gDevice, zeroBuffer.data(), zeroBuffer.size());
    auto values = createBuffer(gDevice, zeroBuffer.data(), zeroBuffer.size());

    // prepare to launch
    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "FaceOrbit",    { "uint", "uint" } } } },
    }));

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    auto computePass = encoder->beginComputePass();

    // triangulate
    auto triangulate = computePass->bindPipeline(kernels[0].pipeline);
    rhi::ShaderCursor triangulateParam(triangulate->getEntryPoint(0));

    dart.writeInto(triangulateParam.getPath("param").getPath("model"));
    triangulateParam.getPath("param").getPath("orbitCounter").setBinding(orbitCounter);
    triangulateParam.getPath("param").getPath("offsetCounter").setBinding(offsetCounter);
    triangulateParam.getPath("param").getPath("indices").setBinding(indices);
    triangulateParam.getPath("param").getPath("values").setBinding(values);

    computePass->dispatchCompute(map.count(), 1, 1);
    
    computePass->end();
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer\n");

    std::vector<u32> outFaceCount = slangrhi::readBuffer<u32>(gDevice, orbitCounter, 1);
    REQUIRE(outFaceCount.size() == 1);

    std::vector<u32> outFaceOffset = slangrhi::readBuffer<u32>(gDevice, indices, outFaceCount[0] + 1);
    REQUIRE(!outFaceOffset.empty());
    std::vector<u32> outFaceVertexList = slangrhi::readBuffer<u32>(gDevice, values, outFaceOffset.back());

    // cpu reference
    // same logic as FaceOrbit in giha_kernel/orbit.slang
    std::vector<u32> expectedOffsets;
    expectedOffsets.push_back(0);
    std::vector<u32> expectedValues;

    auto faceNext = [&](u32 dartID) -> u32 {
        return map.vNext[map.eNext[dartID]];
    };

    for (u32 dartID = 0; dartID < map.count(); ++dartID) {
        u32 curr = dartID;
        u32 minID = dartID;
        std::vector<u32> orbit;
        do {
            orbit.push_back(curr);
            minID = std::min(minID, curr);
            curr = faceNext(curr);
        } while (curr != dartID);

        if (dartID != minID) continue;

        expectedOffsets.push_back(expectedOffsets.back() + static_cast<u32>(orbit.size()));
        for (u32 dartInOrbit : orbit) {
            expectedValues.push_back(map.vKeys[dartInOrbit]);
        }
    }

    const u32 expectedFaceCount = static_cast<u32>(expectedOffsets.size() - 1);

    REQUIRE(outFaceCount[0] == expectedFaceCount);
    REQUIRE(outFaceOffset.size() == expectedOffsets.size());
    REQUIRE(outFaceVertexList.size() == expectedValues.size());
    REQUIRE(std::equal(outFaceOffset.begin(), outFaceOffset.end(), expectedOffsets.begin()));
    REQUIRE(std::equal(outFaceVertexList.begin(), outFaceVertexList.end(), expectedValues.begin()));
}

// 
TEST_IN(GeometrySuite, BuildGeometryQuantities) {

    using Scalar = f32;

    const std::string path = gTestPath + "/resource/tetrahedron.ply";
    const auto& polymesh = gGeometryAssets.mesh<Scalar, u32>(path);
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(path);

    ExtrinsicGeometry extrinsic(gDevice, map, (void*)polymesh.vertexCoordinates.data(), sizeof(polymesh.vertexCoordinates[0]) * polymesh.vertexCoordinates.size());
    IntrinsicGeometry intrinsic(gDevice, map, map.count() * sizeof(Scalar));

    // prepare to launch
    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "BuildGeometry", { "uint", "float" } } } },
    }));

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    auto computePass = encoder->beginComputePass();

    // triangulate
    auto triangulate = computePass->bindPipeline(kernels[0].pipeline);
    rhi::ShaderCursor buildGeometryParam(triangulate->getEntryPoint(0));

    extrinsic.writeInto(buildGeometryParam.getPath("param").getPath("extrinsic"));
    intrinsic.writeInto(buildGeometryParam.getPath("param").getPath("intrinsic"));

    computePass->dispatchCompute(map.count(), 1, 1);

    computePass->end();
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer\n");

    std::vector<Scalar> outIntrinsicLength = slangrhi::readBuffer<Scalar>(gDevice, intrinsic.dartLengths, map.count());
    for (u32 dartID = 0; dartID < map.count(); ++dartID) {

        // check tetrahedron edge length
        REQUIRE(outIntrinsicLength[dartID] == Scalar(2.828427));
    }
}

TEST_IN(GeometrySuite, LoopSubdivide) {

    using Scalar = f32;

    const std::string path = gTestPath + "/resource/tetrahedron.ply";
    const auto& polymesh = gGeometryAssets.mesh<Scalar, u32>(path);
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(path);

    MutableSigmaModel dart(gDevice, map, 100); // @@todo padding for new darts
    MutableExtrinsicGeometry extrinsic(gDevice, dart, (Scalar*)polymesh.vertexCoordinates.data(), polymesh.vertexCoordinates.size());

    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "LoopSubdivide", { "uint", "float" } } } },
        { "module/giha_kernel.slang", { { "FaceOrbit", { "uint", "float" } } } },
    })) ;

    printf("somme\n");
    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    auto computePass = encoder->beginComputePass();

    // loop subdivide
    {
        auto subdivide = computePass->bindPipeline(kernels[0].pipeline);
        rhi::ShaderCursor subdivideParam(subdivide->getEntryPoint(0));
        extrinsic.writeInto(subdivideParam.getPath("extrinsic"));
        computePass->dispatchCompute(map.count(), 1, 1);
        computePass->end();
    }

    // face orbit
    {
        auto orbit = computePass->bindPipeline(kernels[1].pipeline);
        rhi::ShaderCursor orbitParam(orbit->getEntryPoint(0));

        dart.writeInto(orbitParam.getPath("param").getPath("model"));
        computePass->dispatchCompute(map.count(), 1, 1);
        computePass->end();
    }
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer\n");

    std::vector<u32> outFaceCount = slangrhi::readBuffer<u32>(gDevice, extrinsic.counter, 1);
    printf("Subdivided face count: %u\n", outFaceCount[0]);
}
