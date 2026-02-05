implementing giha_kernel;

struct BuildGeometryParam<I : IInteger & IAtomicable, Scalar : __BuiltinFloatingPointType>
{
    ExtrinsicGeometry<I, Scalar> extrinsic;
    MutableIntrinsicGeometry<I, Scalar> intrinsic;
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void BuildGeometry<I : IInteger & IAtomicable, Scalar : __BuiltinFloatingPointType>(
    ParameterBlock<BuildGeometryParam<I, Scalar>> param,
    uint3 gid : SV_DispatchThreadID
)
{
    if (gid.x >= param.extrinsic.model.count()) return;

    I dartID = I(gid.x);
    param.intrinsic.length(dartID, param.extrinsic);
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void flip()
{
}