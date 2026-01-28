implementing giha_kernel;

struct PrimiviteOrbitParam<I : IInteger, K : IArithmetic>
{
    SigmaModel<I, K> model;
    RWStructuredBuffer<Atomic<uint>> orbitCounter;
    RWStructuredBuffer<Atomic<uint>> offsetCounter;
    RWStructuredBuffer<uint> indices;
    RWStructuredBuffer<K> values;
};

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void VertexOrbit<I : IInteger, K : IArithmetic>(
    uniform ParameterBlock<PrimiviteOrbitParam<I, K>> param,
    uint3 tid: SV_GroupThreadID,
    uint3 gid: SV_DispatchThreadID
)
{
    if (gid.x >= param.model.count()) return;

    I dartID = I(gid.x);
    let collect = param.model.collectVertOrbitIfMin(dartID, tid, param.orbitCounter, param.offsetCounter, param.indices);
    if (!collect.isValid()) return;

    let base = param.indices[collect.orbitID.toInt()];
    for (int localID = 0; localID < collect.count(); ++localID)
    {
        var orbitDartID = collect.get(localID);
        param.values[base + localID] = param.model.mapToExtrinsic(orbitDartID);
    }
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void EdgeOrbit<I : IInteger, K : IArithmetic>(
    uniform ParameterBlock<PrimiviteOrbitParam<I, K>> param,
    uint3 tid: SV_GroupThreadID,
    uint3 gid: SV_DispatchThreadID
)
{
    if (gid.x >= param.model.count()) return;
    I dartID = I(gid.x);
    let collect = param.model.collectEdgeOrbitIfMin(dartID, tid, param.orbitCounter, param.offsetCounter, param.indices);
    if (!collect.isValid()) return;

    let base = param.indices[collect.orbitID.toInt()];
    for (int localID = 0; localID < collect.count(); ++localID)
    {
        var orbitDartID = collect.get(localID);
        param.values[base + localID] = param.model.mapToExtrinsic(orbitDartID);
    }
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void FaceOrbit<I : IInteger, K : IArithmetic>(
    uniform ParameterBlock<PrimiviteOrbitParam<I, K>> param,
    uint3 tid: SV_GroupThreadID,
    uint3 gid: SV_DispatchThreadID
)
{
    if (gid.x >= param.model.count()) return;

    I dartID = I(gid.x);
    let collect = param.model.collectFaceOrbitIfMin(dartID, tid, param.orbitCounter, param.offsetCounter, param.indices);
    if (!collect.isValid()) return;

    let base = param.indices[collect.orbitID.toInt()];    
    for (int localID = 0; localID < collect.count(); ++localID)
    {
        var orbitDartID = collect.get(localID);
        param.values[base + localID] = param.model.mapToExtrinsic(orbitDartID);
    }
}