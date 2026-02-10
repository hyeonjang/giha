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
        uint tail = model.mapToVertexKey(dartID);
        uint head = model.mapToVertexKey(model.nextEdge(dartID));

        return length(vertexPositions[tail] - vertexPositions[head]);
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
    public RWStructuredBuffer<vector<Scalar, 2>> dartTransports;

    // 
    // quantity computation
    // 
    public Scalar computeLength(I dartID, ExtrinsicGeometry<I, Scalar> extrinsic)
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

    public bool edgeFlipIfNotDelaunay(I i0)
    {
        I curr = i0;
        int prevFlag = 0;
        if (cotanWeight(curr) > -Scalar(1e-6))
            InterlockedCompareExchange(model.ownershipFlags[curr.toInt()], 0, int(curr.toInt()), prevFlag);

        GroupMemoryBarrierWithGroupSync();

        //@@ more sophisticate to do this
        // check atomic
        I prev = model.prevVert(curr);
        if (model.ownershipFlags[prev.toInt()] != 0 )
        {
            I mn = curr < prev ? curr : prev;
            if (mn != curr)
            {
                // return the ownership
                InterlockedCompareExchange(model.ownershipFlags[curr.toInt()], int(curr.toInt()), 0, prevFlag);
                return false;
            }
        }

        I next = model.nextVert(curr);
        if (model.ownershipFlags[next.toInt()] != 0)
        {
            I mn = curr < next ? curr : next;
            if (mn != curr)
            {
                // return the ownership
                InterlockedCompareExchange(model.ownershipFlags[curr.toInt()], int(curr.toInt()), 0, prevFlag);
                return false;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        // check whether target
        I twin = model.nextEdge(curr);
        if (model.ownershipFlags[curr.toInt()] != 0 && model.ownershipFlags[twin.toInt()] != 0)
        {
        // we can also do by only inducedMap
            I fPrev = model.atomicRemoveVertNext(curr);
            model.atomicInsertVertNext(fPrev, curr);
            model.setVertexKey(curr, model.mapToVertexKey(fPrev));

            // @@ compute new length
            // dartLengths[curr.toInt()] = 
        }
        GroupMemoryBarrierWithGroupSync();

        // return the ownership
        InterlockedCompareExchange(model.ownershipFlags[curr.toInt()], int(curr.toInt()), 0, prevFlag);
        return true;
    }
}
