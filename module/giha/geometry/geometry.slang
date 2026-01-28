implementing giha;

public interface IQuantities
{
}

public struct ExtrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public SigmaModel<I, uint> model;

    // should be given always
    public uint counter;
    public StructuredBuffer<vector<Scalar, 3>> vertexPositions;

    public Scalar computeEdgeLength(I dartID)
    {
        uint tail = model.mapToExtrinsic(dartID);
        uint head = model.mapToExtrinsic(model.nextEdge(dartID));

        return length(vertexPositions[tail] - vertexPositions[head]);
    }
}

public struct MutableExtrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public MutableSigmaModel<I, uint> model;

    public RWStructuredBuffer<Atomic<uint>> counter;
    public RWStructuredBuffer<vector<Scalar, 3>> vertexPositions;

    public Scalar computeEdgeLength(I dartID)
    {
        uint tail = model.mapToExtrinsic(dartID);
        uint head = model.mapToExtrinsic(model.nextEdge(dartID));

        return length(vertexPositions[tail] - vertexPositions[head]);
    }
}

public struct IntrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public SigmaModel<I, uint> model;

    public uint counter;
    public StructuredBuffer<Scalar> dartLengths;
}

public struct MutableIntrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public MutableSigmaModel<I, uint> model;

    public Atomic<uint> counter;
    public RWStructuredBuffer<Scalar> dartLengths;

    public Scalar computeDartLength(I dartID, ExtrinsicGeometry<I, Scalar> extrinsic)
    {
        dartLengths[dartID.toInt()] = extrinsic.computeEdgeLength(dartID);
        return dartLengths[dartID.toInt()];
    }
}
