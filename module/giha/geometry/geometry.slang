implementing giha;

public struct ExtrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public SigmaModel<I, uint, StructuredBuffer<I>, StructuredBuffer<uint>> model;

    // should be given always
    public uint counter;
    public StructuredBuffer<vector<Scalar, 3>> vertexPositions;

    public Scalar computeEdgeLength(I dartID)
    {
        uint tail = model.mapToVertexKey(dartID);
        uint head = model.mapToVertexKey(model.nextEdge(dartID));

        return length(vertexPositions[tail] - vertexPositions[head]);
    }
}

public struct MutableExtrinsicGeometry<I : IInteger & IAtomicable, Scalar : __BuiltinFloatingPointType>
{
    public MutableSigmaModel<I, uint> model;

    public RWStructuredBuffer<Atomic<uint>> counter;
    public RWStructuredBuffer<vector<Scalar, 3>> vertexPositions;

    public Scalar computeEdgeLength(I dartID)
    {
        // uint tail = model.mapToVertexKey(dartID);
        // uint head = model.mapToVertexKey(model.nextEdge(dartID));

        // return length(vertexPositions[tail] - vertexPositions[head]);
    }
}

public struct IntrinsicGeometry<I : IInteger, Scalar : __BuiltinFloatingPointType>
{
    public SigmaModel<I, uint, StructuredBuffer<I>, StructuredBuffer<uint>> model;
    public StructuredBuffer<Scalar> dartLengths;
}

public struct MutableIntrinsicGeometry<I : IInteger & IAtomicable, Scalar : __BuiltinFloatingPointType>
{
    public MutableSigmaModel<I, uint> model;
    public RWStructuredBuffer<Scalar> dartLengths;

    // 
    // quantity computation
    // 
    public Scalar length(I dartID, ExtrinsicGeometry<I, Scalar> extrinsic)
    {
        dartLengths[dartID.toInt()] = extrinsic.computeEdgeLength(dartID);
        return dartLengths[dartID.toInt()];
    }

    public Scalar cotanWeight(I i0)
    {
        I i1 = model.nextFace(i0);
        I i2 = model.nextFace(i1);

        Scalar a = dartLengths[i0.toInt()];
        Scalar b = dartLengths[i1.toInt()];
        Scalar c = dartLengths[i2.toInt()];

        // face area
        Scalar s = (a + b + c) * Scalar(0.5);
        Scalar areaSq = max(Scalar(0.0), s * (s - a) * (s - b) * (s - c));
        Scalar area = sqrt(areaSq); // actual triangle area

        // @@ check quantities... it is halfedge
        return (b * b + c * c - a * a) / (Scalar(4.0) * area);
    }

    public Scalar faceArea(I i0)
    {
        I i1 = model.nextFace(i0);
        I i2 = model.nextFace(i1);

        Scalar a = dartLengths[i0.toInt()];
        Scalar b = dartLengths[i1.toInt()];
        Scalar c = dartLengths[i2.toInt()];

        // Heron's formular
        Scalar s = (a + b + c) * Scalar(0.5);
        Scalar areaSq = max(Scalar(0.0), s * (s - a) * (s - b) * (s - c));
        return sqrt(areaSq);
    }

    //
    // orbit
    // 
    public bool edgeFlipIfNotDelaunay(I i0)
    {
        // let edgeCollect = model.collect        

        // check delaunay
        if (cotanWeight(i0) > -Scalar(1e-6)) return false;

        let layoutPositions = layoutDiamond(i0);
        let newLength = length(layoutPositions[1] - layoutPositions[3]);

        // 1. atomic splice
        I i1 = model.nextVert(i0);
        // combinatorial map
        // atomic insertion after splice would be well working
        // if (!model.atomicRotate(i0)) return false;

        // 2. check changed by others
        I i2 = model.nextVert(i0);
        if (i1 == i2)
        {
            return true;
        }
        GroupMemoryBarrierWithGroupSync();

        // else when modified, retry


        dartLengths[i0.toInt()] = newLength;

        return true;
    }

    private Array<vector<Scalar, 2>, 4> layoutDiamond(I i0)
    {
        I a0 = i0;  
        I a1 = model.nextFace(a0);
        I a2 = model.nextFace(a1);
    }
}
