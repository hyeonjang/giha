implementing giha_kernel;

// subdivision test purpose
// : no boundary consideration
[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void LoopSubdivide<I : IInteger & IAtomicable, Scalar : __BuiltinFloatingPointType>(
    uniform MutableExtrinsicGeometry<I, Scalar> extrinsic,
    uint3 tid: SV_GroupThreadID,
    uint3 gid: SV_DispatchThreadID
)
{
    if (gid.x >= extrinsic.model.count()) return;

    // original face orbit : d0 -> d1 -> d2
    // original edge orbit : d0 -> de0
    I d0 = I(gid.x);

    // 1. store for later use
    I df1 = extrinsic.model.nextFace(d0); // next face
    I de1 = extrinsic.model.nextEdge(d0);  // twin edge

    // 1-1. for reposition calculation
    I df2 = extrinsic.model.nextFace(df1); // next-next face
    I de1f2 = extrinsic.model.nextFace(extrinsic.model.nextFace(de1));

    // new vertex postiion
    // some interpolation

    // 2. insertion to inner face
    // new edge orbit : d0 -> de0 -> de1 ->
    I de0;
    extrinsic.model.insertNewDartOnEdge(d0, de0);

    // new vert orbit : de1 -> dv0 -> dv1
    I dv0;
    extrinsic.model.insertNewDartOnVert(de0, dv0);

    I dv1;
    extrinsic.model.insertNewDartOnVert(dv0, dv1);
    GroupMemoryBarrierWithGroupSync();

    // 3. connect to other edge on faces, consistent within triangle
    // 3-1. next face
    I df1e0 = extrinsic.model.nextEdge(df1);
    I df1e0v0 = extrinsic.model.nextVert(df1e0);
    extrinsic.model.setEdgeNext(df1e0v0, dv1);
    extrinsic.model.setEdgeNext(dv1, df1e0v0);

    // 4. select thread to assign extrinsic key
    if (d0 > de1) return;
    I de1e0 = extrinsic.model.nextEdge(de1);
    I de1e0v0 = extrinsic.model.nextVert(de1e0);
    I de1e0v1 = extrinsic.model.nextVert(de1e0v0);

    // 5. position a new vertex on edge
    uint newKey = extrinsic.counter[0].increment();
    extrinsic.model.setVertexKey(de0, newKey);
    extrinsic.model.setVertexKey(dv0, newKey);
    extrinsic.model.setVertexKey(dv1, newKey);

    // 5-1. mirror side
    extrinsic.model.setVertexKey(de1e0, newKey);
    extrinsic.model.setVertexKey(de1e0v0, newKey);
    extrinsic.model.setVertexKey(de1e0v1, newKey);

    var mid0 = extrinsic.vertexPositions[extrinsic.model.mapToVertex(d0)] + extrinsic.vertexPositions[extrinsic.model.mapToVertex(de1)];
    var mid1 = extrinsic.vertexPositions[extrinsic.model.mapToVertex(df2)] + extrinsic.vertexPositions[extrinsic.model.mapToVertex(de1f2)];

    mid0 = mid0 * Scalar(3.0) / Scalar(8.0);
    mid1 = mid1 / Scalar(8.0);

    extrinsic.vertexPositions[newKey] = mid0 + mid1;
    
    // 6. reposition old vertex

}
