import giha;

#define Index int
#define Key int

ParameterBlock<DartBasis<int, int>> basis;

    // // simplicial complex operator
// [numthreads(256, 1, 1)]
// void delaunayFlip(
//     uint3 gid: SV_DispatchThreadID
// ) {
//     int dart_id = gid.x;

//     // // mask indices to flip
//     // float dart_length;
//     // if (dart_length > 1e-3) {
//     //     return;
//     // }

//     basis.removeVertexNext(dart_id);
// }

vector<I, 3> cyclicOrder<I : IInteger>(vector<I, 3> values) 
{
    I minIndex = I(0);
    I minValue = values.x;

    if (values.y < minValue) {
        minValue = values.y;
        minIndex = I(1);
    }
    if (values.z < minValue) {
        minValue = values.z;
        minIndex = I(2);
    }

    if (minIndex == I(0)) {
        return values;
    } else if (minIndex == I(1)) {
        return vector<I, 3>(values.y, values.z, values.x);
    } else {
        return vector<I, 3>(values.z, values.x, values.y);
    }
}

struct TriangulateParam
{
    RWStructuredBuffer<vector<int, 3>> faceVertexList;
    RWStructuredBuffer<vector<int, 3>> faceVertexScratch;
    RWStructuredBuffer<uint> faceVertexCount;
};

[numthreads(, 1, 1)]
void triangulate(
    uniform ParameterBlock<TriangulateParam> param, // drop `uniform` so it’s mutable
    uint3 gid: SV_DispatchThreadID)
{
} 
