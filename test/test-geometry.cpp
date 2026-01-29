#include "suite-test.hpp"

#include <slang-rhi.h>

#include <giha/slang/slang.h>
#include <giha/slang/adapters/slang-rhi.hpp>
#include <giha/geometry/adapters/slang-rhi/kernel.h>
#include <giha/geometry/happly.h>

#include <array>

#include "giha/geometry-assets.h"

DECLARE_SUITE(GeometrySuite);
using namespace giha;
using namespace giha::slangrhi;

extern Slang::ComPtr<rhi::IDevice> gDevice;
static giha::SlangKernelCache gKernelCache;
extern const std::string gTestPath;

using Scalar = f32;
using Id = u32;

namespace {

void writeSubdividedMeshToPly(
    const std::string& path,
    const std::vector<Scalar>& vertexCoords,
    const std::vector<u32>& faceOffsets,
    const std::vector<u32>& faceIndices
) {
    if (vertexCoords.empty()) {
        printf("Skip writing %s: no vertices\n", path.c_str());
        return;
    }

    const size_t vertexCount = vertexCoords.size() / 3;
    std::vector<std::array<double, 3>> positions(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        positions[i][0] = static_cast<double>(vertexCoords[i * 3 + 0]);
        positions[i][1] = static_cast<double>(vertexCoords[i * 3 + 1]);
        positions[i][2] = static_cast<double>(vertexCoords[i * 3 + 2]);
    }

    std::vector<std::vector<u32>> faces;
    if (faceOffsets.size() > 1) {
        faces.reserve(faceOffsets.size() - 1);
        for (size_t faceIndex = 0; faceIndex + 1 < faceOffsets.size(); ++faceIndex) {
            const u32 start = faceOffsets[faceIndex];
            const u32 end = faceOffsets[faceIndex + 1];
            if (end <= start || end > faceIndices.size()) { continue; }
            faces.emplace_back(faceIndices.begin() + start, faceIndices.begin() + end);
        }
    }

    happly::PLYData plyOut;
    plyOut.addVertexPositions(positions);
    if (!faces.empty()) {
        plyOut.addFaceIndices(faces);
    }
    plyOut.write(path, happly::DataFormat::ASCII);
    printf("Wrote subdivided mesh to %s (%zu vertices, %zu faces)\n", path.c_str(), positions.size(), faces.size());
}

} // namespace

// 0. prepare slang session && @@todo loading test model
TEST_IN(GeometrySuite, PrepareGeometrySession) {
    gKernelCache.reset(gDevice->getSlangSession());
}

// 1. dart map (sigma model)
TEST_IN(GeometrySuite, BuildDartMap) {
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(gTestPath + "/resource/octahedron.ply");

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
    const auto& polymesh = gGeometryAssets.mesh<Scalar, u32>(path);
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(path);

    using VertexIdType = u32;

    // prepare uniform buffer
    SigmaModel model(gDevice, map);
    FaceOrbitParam orbitParam(gDevice, model);

    // prepare to launch
    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "FaceOrbit",    { "uint", "uint" } } } },
    }));

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    auto computePass = encoder->beginComputePass();

    // triangulate
    {
        auto triangulate = computePass->bindPipeline(kernels[0].pipeline);
        rhi::ShaderCursor triangulateParam(triangulate->getEntryPoint(0));

        orbitParam.writeInto(triangulateParam.getPath("param"));
        computePass->dispatchCompute(map.count(), 1, 1);
        computePass->end();
        CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer\n");
    }

    // check results
    {
        std::vector<u32> outFaceCount = slangrhi::readBuffer<u32>(gDevice, orbitParam.orbitCounter, 1);
        std::vector<u32> outFaceOffset = slangrhi::readBuffer<u32>(gDevice, orbitParam.indices, outFaceCount[0] + 1);
        std::vector<u32> outFaceVertexList = slangrhi::readBuffer<u32>(gDevice, orbitParam.values, outFaceOffset.back());

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
}

// 
TEST_IN(GeometrySuite, BuildGeometryQuantities) {

    const std::string path = gTestPath + "/resource/tetrahedron.ply";
    const auto& polymesh = gGeometryAssets.mesh<Scalar, u32>(path);
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(path);

    SigmaModel model(gDevice, map);
    ExtrinsicGeometry extrinsic(gDevice, model, polymesh.vertexCoordinates.data(), polymesh.vertexCoordinates.size());
    IntrinsicGeometry intrinsic(gDevice, model, (Scalar*)nullptr, map.count());

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

    const std::string path = gTestPath + "/resource/tetrahedron.ply";
    const auto& polymesh = gGeometryAssets.mesh<Scalar, u32>(path);
    const auto& map = gGeometryAssets.dartMap<Scalar, u32>(path);

    SigmaModel dart(gDevice, map, 36); // @@todo padding for new darts
    ExtrinsicGeometry extrinsic(gDevice, dart, (Scalar*)polymesh.vertexCoordinates.data(), polymesh.vertexCoordinates.size(), 4);
    FaceOrbitParam orbitParam(gDevice, dart);

    auto kernels = slangrhi::makeComputeKernels(gDevice, gKernelCache.kernels({
        { "module/giha_kernel.slang", { { "LoopSubdivide", { "uint", "float" } } } },
        { "module/giha_kernel.slang", { { "FaceOrbit", { "uint", "float" } } } },
    })) ;

    auto queue = gDevice->getQueue(rhi::QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();

    auto computePass = encoder->beginComputePass();
    {
        // loop subdivide
        auto subdivide = computePass->bindPipeline(kernels[0].pipeline);
        rhi::ShaderCursor subdivideParam(subdivide->getEntryPoint(0));
        extrinsic.writeInto(subdivideParam.getPath("extrinsic"));
        computePass->dispatchCompute(map.count(), 1, 1);

        // face orbit
        auto orbit = computePass->bindPipeline(kernels[1].pipeline);
        rhi::ShaderCursor orbitCursor(orbit->getEntryPoint(0));
        orbitParam.writeInto(orbitCursor.getPath("param"));
        // computePass->dispatchComputeIndirect(dart.counter);
        computePass->dispatchCompute(48, 1, 1);
    }
    computePass->end();
    CHECK_SLANG(queue->submit(encoder->finish()), "Failed to submit command buffer\n");
    printf("Submitted\n");

    std::vector<u32> dartCounter = slangrhi::readBuffer<u32>(gDevice, dart.counter, 1);
    printf("Dart count after subdivision: %u\n", dartCounter[0]);

    std::vector<u32> dartVertNext = slangrhi::readBuffer<u32>(gDevice, dart.vertNext, dartCounter[0]);
    for (u32 i = 0; i < dartCounter[0]; ++i) {
        printf("Dart %u -> Vertex %u\n", i, dartVertNext[i]);
    }

    std::vector<u32> dartEdgeNext = slangrhi::readBuffer<u32>(gDevice, dart.edgeNext, dartCounter[0]);
    for (u32 i = 0; i < dartCounter[0]; ++i) {
        printf("Dart %u -> Twin Dart %u\n", i, dartEdgeNext[i]);
    }


    std::vector<u32> outFaceCount = slangrhi::readBuffer<u32>(gDevice, orbitParam.orbitCounter, 1);
    printf("Face count: %u\n", outFaceCount[0]);
    std::vector<u32> outFaceOffset = slangrhi::readBuffer<u32>(gDevice, orbitParam.indices, outFaceCount[0] + 1);
    std::vector<u32> outFaceVertexList = slangrhi::readBuffer<u32>(gDevice, orbitParam.values, outFaceOffset.back());

    std::vector<u32> outVertexCount = slangrhi::readBuffer<u32>(gDevice, extrinsic.counter, 1);
    printf("Vertex count: %u\n", outVertexCount[0]);
    std::vector<Scalar> outVertexCoordinates = slangrhi::readBuffer<f32>(gDevice, extrinsic.vertexPositions, outVertexCount[0] * 3);

    const std::string outPath = gTestPath + "/resource/triangle_subdivided.ply";
    writeSubdividedMeshToPly("triangulate.ply", outVertexCoordinates, outFaceOffset, outFaceVertexList);
}
