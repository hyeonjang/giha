#include "suite-test.hpp"
#include "giha/slang-util.hpp"

#include <slang-rhi.h>
#include <giha/slang/slang.h>
#include <giha/geometry/dart.h>
#include <giha/geometry/happly.h>
#include <giha/geometry/polygonmesh.h>

DECLARE_SUITE(GeometrySuite);
using namespace giha;

extern Slang::ComPtr<rhi::IDevice> gDevice;
extern const std::string gTestPath;

TEST_IN(GeometrySuite, triangulate) {

    // cpu prepare
    auto polymesh = giha::PolygonMesh<float, uint32_t>::load(gTestPath + "/resource/vertex-incident-face.ply");

    const auto& faceVertexIncidence = polymesh.polygons;
    auto dartMapFactory = giha::DartMapFactory(faceVertexIncidence);
    auto map = DartMap<uint32_t, uint32_t>::fromVertexVertexAdjacency(dartMapFactory.vertexVertexAdjacency);

    // gpu prepare
    {
        giha::SlangSession session(gDevice->getSlangSession());
        // auto kernel = session.addKernel("dart.cs.slang", { "triangulate" });

        // giha::SlangRHIKernel rhiKernel(gDevice, kernel);
        // DartBlock dartBlock(gDevice, map);

        // using HashKey = i32vec3;

        // // kernel parameters
        // struct TriangulateParam {
        //     Slang::ComPtr<rhi::IBuffer> faceVertexList;
        //     Slang::ComPtr<rhi::IBuffer> faceVertexScratch;
        //     Slang::ComPtr<rhi::IBuffer> faceVertexCount;
            
        //     void writeInto(rhi::ShaderCursor cursor) const {
        //         cursor.getPath("faceVertexList").setBinding(faceVertexList);
        //         cursor.getPath("faceVertexScratch").setBinding(faceVertexScratch);
        //         cursor.getPath("faceVertexCount").setBinding(faceVertexCount);
        //     }
        // };

        // // param
        // uint32_t zero = 0;
        // const int faceCount = map.count();
        // const size_t faceStride = 3 * sizeof(int);
        // const size_t estimatedUniqueFaces = map.count();            // or tighter bound if known
        // const size_t slotBudget = 2 * estimatedUniqueFaces;             // 50% load factor

        // TriangulateParam param {
        //     .faceVertexList = createUniformBuffer(gDevice, nullptr, faceCount * faceStride),
        //     .faceVertexScratch = createUniformBuffer(gDevice, nullptr, faceCount * faceStride),
        //     .faceVertexCount = createUniformBuffer(gDevice, &zero, sizeof(uint32_t)),
        // };
    
        //     gDevice, rhiKernel, 1,
        //     { ParamBlock::bind(&dartBlock) },
        //     { ParamBlock::bind(&param) }
        // );
    }

    // uint32_t zero = 0;
    // const int faceCount = permute.count();
    // const size_t faceStride = sizeof(int) * 3;
    // const size_t estimatedUniqueFaces = permute.count();            // or tighter bound if known
    // const size_t slotBudget = 2 * estimatedUniqueFaces;             // 50% load factor

    // TriangulateParam param {
    //     .hash = HashBlock<Key, int>(gDevice, slotBudget),
    //     .faceVertexList = createUniformBuffer(gDevice, nullptr, faceCount * faceStride, faceStride),
    //     .faceVertexCount = createUniformBuffer(gDevice, &zero, sizeof(uint32_t), sizeof(uint32_t)),
    // };

    // dispatchKernel(gDevice, rhiKernel, permute.count(),
    //     { ParamBlock::bind(&dartBlock) },
    //     { ParamBlock::bind(&param) }
    // );


    // Slang::ComPtr<slang::IBlob> counter;
    // CHECK_SLANG(gDevice->readBuffer(param.faceVertexCount, 0, sizeof(uint32_t), counter.writeRef()), "Failed to readback");
    // const uint32_t uniqueCount = reinterpret_cast<const uint32_t*>(counter->getBufferPointer())[0];
    // printf("%d %u\n", faceCount, uniqueCount);
    // // CHECK(faceCount == ((int*)counter->getBufferPointer())[0], "False");

    // Slang::ComPtr<ISlangBlob> blob;
    // CHECK_SLANG(gDevice->readBuffer(param.faceVertexList, 0, faceCount * faceStride, blob.writeRef()), "");

    // auto pointer = (int*)blob->getBufferPointer();
    // std::vector<std::vector<size_t>> faceIndices(uniqueCount);
    // for (uint32_t i = 0; i < uniqueCount; i++) {
    //     for (int j = 0; j < 3; j++)        
    //         faceIndices[i].push_back(pointer[3* i + j]);
    // }

    // auto vertexPositions = ply.getVertexPositions();

        
    // auto factory = DartPermuteFactory<int>(ply.getFaceIndices<int>(), vertexPositions.size());
    // factory.orderCyclicVertexFaceIncidence();
    // factory.transposeVertexFaceIncidence();

    // auto faceVertexIncidence = permute.buildFaceVertexIncidence();

    // happly::PLYData plyOut;
    // plyOut.addVertexPositions(vertexPositions);
    // // plyOut.addFaceIndices(faceIndices);
    // plyOut.addFaceIndices(faceVertexIncidence);
    // // plyOut.addFaceIndices(factory.faceVertexIncidence);

    // plyOut.write("triangulate.ply", happly::DataFormat::ASCII);
}
