implementing giha_kernel;

struct BuildGeometryParam<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    ExtrinsicGeometry<I, Scalar> extrinsic;
    MutableIntrinsicGeometry<I, Scalar> intrinsic;
}

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void BuildGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>(
    ParameterBlock<BuildGeometryParam<I, Scalar>> param,
    uint3 gid : SV_DispatchThreadID
)
{
    if (gid.x >= param.extrinsic.model.count()) return;

    I dartID = I(gid.x);
    param.intrinsic.computeDartLength(dartID, param.extrinsic);
}