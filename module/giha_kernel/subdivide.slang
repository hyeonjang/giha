implementing giha_kernel;

// subdivision test purpose
// : only care about topology
[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void LoopSubdivide<I : IInteger, Scalar : __BuiltinFloatingPointType>(
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
    I df1 = extrinsic.model.nextFace(d0);  // next face
    I df2 = extrinsic.model.nextFace(df1); // next-next face
    I de1 = extrinsic.model.nextEdge(d0);  // twin edge

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

    // 3. connect to other edge on faces
    // 3-1. next face
    I df1e0 = extrinsic.model.nextEdge(df1);
    I df1e0v0 = extrinsic.model.nextVert(df1e0);
    extrinsic.model.edgeNext[df1e0v0.toInt()] = dv1;
    extrinsic.model.edgeNext[dv1.toInt()] = df1e0v0;

    // 3-2. next-next face
    I df2e0 = extrinsic.model.nextEdge(df2);
    I df2e0v1 = extrinsic.model.nextVert(df2e0);
    extrinsic.model.edgeNext[df2e0v1.toInt()] = dv0;
    extrinsic.model.edgeNext[dv0.toInt()] = df2e0v1;

    // 4. select thread to assign extrinsic key
    if (d0 > de1) return;
    I de1e0 = extrinsic.model.nextEdge(de1);
    I de1e0v0 = extrinsic.model.nextVert(de1e0);
    I de1e0v1 = extrinsic.model.nextVert(de1e0v0);

    uint newKey = extrinsic.counter[0].increment();
    extrinsic.model.assignExtrinsic(de0, newKey);
    extrinsic.model.assignExtrinsic(dv0, newKey);
    extrinsic.model.assignExtrinsic(dv1, newKey);
    extrinsic.model.assignExtrinsic(de1e0, newKey);
    extrinsic.model.assignExtrinsic(de1e0v0, newKey);
    extrinsic.model.assignExtrinsic(de1e0v1, newKey);

    extrinsic.vertexPositions[newKey] = lerp(
        extrinsic.vertexPositions[extrinsic.model.mapToExtrinsic(d0)],
        extrinsic.vertexPositions[extrinsic.model.mapToExtrinsic(de1)],
        Scalar(0.5)
    );
}