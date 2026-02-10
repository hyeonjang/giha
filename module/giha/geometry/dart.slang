implementing giha;

#define GIHA_VERT_ORBIT_MAX_STEPS 24
#define GIHA_EDGE_ORBIT_MAX_STEPS 6
#define GIHA_FACE_ORBIT_MAX_STEPS 6
#define GIHA_COLLECT_MAX_STEPS 24

// static const to share with module
public static const uint VERT_ORBIT_MAX_STEPS = GIHA_VERT_ORBIT_MAX_STEPS;
public static const uint EDGE_ORBIT_MAX_STEPS = GIHA_EDGE_ORBIT_MAX_STEPS;
public static const uint FACE_ORBIT_MAX_STEPS = GIHA_FACE_ORBIT_MAX_STEPS;

interface INextOP<I : IInteger>
{
    static const uint MaxSteps;
    I next(I i);
}

struct VertNext<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = VERT_ORBIT_MAX_STEPS;

    I next(I i) { return vertNext[i.toInt()]; }

    __init(IArray<I> _vertNext)
    {
        vertNext = _vertNext;
    }

    IArray<I> vertNext;
}

struct EdgeNext<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = VERT_ORBIT_MAX_STEPS;
    I next(I i) { return edgeNext[i.toInt()]; }

    __init(IArray<I> _edgeNext)
    {
        edgeNext = _edgeNext;
    }

    IArray<I> edgeNext;
}

struct FaceNext<I : IInteger, Buffer : IArray<I>> : INextOP<I>
{
    static const uint MaxSteps = FACE_ORBIT_MAX_STEPS;
    I next(I i) { return vertNext[edgeNext[i.toInt()].toInt()]; }

    __init(Buffer _vertNext, Buffer _edgeNext)
    {
        vertNext = _vertNext;
        edgeNext = _edgeNext;
    }

    Buffer vertNext;
    Buffer edgeNext;
}

public interface IOrbitHandler<I : IInteger>
{
    public uint count();

    // bool begin(I);
    [mutating]
    bool visit(I dartID, uint steps);
}

public struct MinIdOrbitHandler<I : IInteger> : IOrbitHandler<I>
{
    public uint count() { return _count; }

    [mutating]
    bool visit(I dartID, uint steps)
    {
        if (minID >= dartID) minID = dartID;
        _count = steps + 1;

        return true;
    }

    public I minID;
    uint _count = 0;
}

public struct OribitCollectBuffer
{
    RWStructuredBuffer<Atomic<uint>> orbitCounter;
    RWStructuredBuffer<Atomic<uint>> offsetCounter;
    RWStructuredBuffer<uint> indices;
}

groupshared int orbitCollectShared[WORKGROUP_SIZE * GIHA_COLLECT_MAX_STEPS];

public struct CollectHandler<I : IInteger> : IOrbitHandler<I>
{
    public uint count() { return _count; }

    public __init(uint localThreadID, uint capacity)
    {
        baseOffset = localThreadID * GIHA_COLLECT_MAX_STEPS;
        maxCapacity = capacity;
    }

    [mutating]
    bool visit(I dartID, uint steps)
    {
        if (steps >= maxCapacity) return false;

        orbitCollectShared[baseOffset + steps] = dartID.toInt();
        _count = steps + 1;

        return true;
    }

    public I get(uint index)
    {
        return I(orbitCollectShared[baseOffset + index]);
    }

    public bool isValid() { return orbitID != I(-1); }

    public I orbitID = I(-1);
    uint baseOffset;
    uint maxCapacity;
    uint _count = 0;
}

public struct SigmaModel<I: IInteger, K: IArithmetic, IBuffer : IArray<I>, KBuffer : IArray<K>>
{
    IBuffer vertNext; // \sigma, v-orbit: vertex rotation, CCW by combinatorial
    IBuffer edgeNext; // \alpha, e-orbit: edge rotation, CCW
    KBuffer inducedVertexMap;

    // map to extrinsic
    public K mapToVertexKey(I i0) { return inducedVertexMap[i0.toInt()]; }
    public uint count() { return vertNext.getCount(); }

    //
    // query
    //
    public I nextVert(I i0)
    {
        return vertNext[i0.toInt()];
    }

    public I nextEdge(I i0)
    {
        return edgeNext[i0.toInt()];
    }

    public I nextFace(I i0)
    {
        return vertNext[edgeNext[i0.toInt()].toInt()];
    }

    public I prevVert(I i0)
    {
        return prev(VertNext<I>(vertNext), i0);
    };

    public I prevEdge(I i0)
    {
        return prev(EdgeNext<I>(edgeNext), i0);
    };

    public I prevFace(I i0)
    {
        return edgeNext[vertNext[i0.toInt()].toInt()];
    }

    // vertex
    public bool vertOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler)
    {
        return walkOrbit(i0, VertNext<I>(vertNext), handler);
    }

    //
    public bool edgeOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler)
    {
        return walkOrbit(i0, EdgeNext<I>(edgeNext), handler);
    }

    // expected to work on dynmaic size
    public bool faceOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler)
    {
        return walkOrbit(i0, FaceNext<I, IBuffer>(vertNext, edgeNext), handler);
    }

    public CollectHandler<I> collectVertOrbitIfMin(
        I dartID, uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices
    )
    {
        return collectOrbitIfRepresentative(VertNext<I>(vertNext), VERT_ORBIT_MAX_STEPS,
                                            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

    public CollectHandler<I> collectEdgeOrbitIfMin(
        I dartID, uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices
    )
    {
        return collectOrbitIfRepresentative(EdgeNext<I>(edgeNext), EDGE_ORBIT_MAX_STEPS,
                                            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

    public CollectHandler<I> collectFaceOrbitIfMin(
        I dartID, uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices
    )
    {
        return collectOrbitIfRepresentative(FaceNext<I, IBuffer>(vertNext, edgeNext), FACE_ORBIT_MAX_STEPS,
                                            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

    public CollectHandler<I> collectVertOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer orbitBuffer)
    {
        return collectVertOrbitIfMin(dartID, tid, orbitBuffer.orbitCounter, orbitBuffer.offsetCounter, orbitBuffer.indices);
    }

    public CollectHandler<I> collectEdgeOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer orbitBuffer)
    {
        return collectEdgeOrbitIfMin(dartID, tid, orbitBuffer.orbitCounter, orbitBuffer.offsetCounter, orbitBuffer.indices);
    }

    public CollectHandler<I> collectFaceOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer orbitBuffer)
    {
        return collectFaceOrbitIfMin(dartID, tid, orbitBuffer.orbitCounter, orbitBuffer.offsetCounter, orbitBuffer.indices);
    }

    //
    // private
    //
    private I prev<NextOP : INextOP<I>>(NextOP nextop, I i0)
    {
        I cursor = i0;
        [unroll] for (uint step = 0; step < INextOP<I>::MaxSteps; ++step)
        {
            I next = nextop.next(cursor);
            if (next == i0) { return cursor; }
            cursor = next;
        }
        return cursor;
    }

    private bool walkOrbit<NextOP, Handler>(I i0, NextOP op, inout Handler handler)
        where NextOP : INextOP<I> where Handler : IOrbitHandler<I>
    {
        I i1 = i0;
        [unroll] for (uint step = 0; step < NextOP::MaxSteps; step++)
        {
            handler.visit(i1, step);

            i1 = op.next(i1);
            if (i0 == i1) { return true; }
        }
        return false;
    }

    private CollectHandler<I> collectOrbitIfRepresentative<NextOP : INextOP<I>>(
        NextOP nextop,
        uint ORBIT_MAX_COUNT,
        I dartID,
        uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices,
    )
    {
        var collect = CollectHandler<I>(tid.x, ORBIT_MAX_COUNT);

        MinIdOrbitHandler<I> handler;
        if (!walkOrbit(dartID, nextop, handler)) return collect;
        if (handler.minID != dartID) return collect;

        // gather collection
        let orbitID = orbitCounter[0].increment();
        if (!walkOrbit(dartID, nextop, collect)) return collect;

        let orbitCount = collect.count();
        uint base = offsetCounter[0].add(orbitCount);

        // prefix sum
        indices[orbitID] = base;
        indices[orbitID + 1] = base + orbitCount;

        collect.orbitID = I(orbitID);
        return collect;
    }
};

public struct MutableSigmaModel<I : IInteger & IAtomicable, K : IArithmetic>
{
    // just same as SigmaModel for read
    RWStructuredBuffer<I> vertNext;
    RWStructuredBuffer<I> edgeNext;
    RWStructuredBuffer<K> inducedVertexMap; // induced map

    SigmaModel<I, K, RWStructuredBuffer<I>, RWStructuredBuffer<K>> model;
    RWStructuredBuffer<Atomic<uint>> counter;
    RWStructuredBuffer<Atomic<I>> vertNextAtomic;
    RWStructuredBuffer<Atomic<I>> edgeNextAtomic;
    RWStructuredBuffer<int> ownershipFlags;

    // map
    public K mapToVertexKey(I i0) { return model.mapToVertexKey(i0); }
    public uint count() { return counter[0].load(); }
    public uint increamentCounter() { return counter[0].increment(); }

    public I nextVert(I i) { return model.nextVert(i); }
    public I nextEdge(I i) { return model.nextEdge(i); }
    public I nextFace(I i) { return model.nextFace(i); }
    public I prevVert(I i) { return model.prevVert(i); }
    public I prevEdge(I i) { return model.prevEdge(i); }
    public I prevFace(I i) { return model.prevFace(i); }
    public bool vertOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler) { return model.vertOrbit(i0, handler); }
    public bool edgeOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler) { return model.edgeOrbit(i0, handler); }
    public bool faceOrbit<Handler : IOrbitHandler<I>>(I i0, inout Handler handler) { return model.faceOrbit(i0, handler); }
    public CollectHandler<I> collectVertOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer buf) { return model.collectVertOrbitIfMin(dartID, tid, buf); }
    public CollectHandler<I> collectEdgeOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer buf) { return model.collectEdgeOrbitIfMin(dartID, tid, buf); }
    public CollectHandler<I> collectFaceOrbitIfMin(I dartID, uint3 tid, OribitCollectBuffer buf) { return model.collectFaceOrbitIfMin(dartID, tid, buf); }

    public void setVertNext(I i0, I i1) { vertNext[i0.toInt()] = i1; }
    public void setEdgeNext(I i0, I i1) { edgeNext[i0.toInt()] = i1; }
    public void setFaceNext(I i0, I i1) { vertNext[edgeNext[i0.toInt()].toInt()] = i1; }
    public void setVertexKey(I dartID, K vertexID) { inducedVertexMap[dartID.toInt()] = vertexID; }

    public I atomicNextVert(I i) { return vertNextAtomic[i.toInt()].load(); }
    public I atomicNextEdge(I i) { return edgeNextAtomic[i.toInt()].load(); }

    // modifier        
    [mutating]
    public I newDart()
    {
        let inc = increamentCounter();
        let newID = I(inc);

        // self-rotation
        vertNext[inc] = I(inc);
        edgeNext[inc] = I(inc);
        return newID;
    }

    [mutating]
    public bool insertNewDartOnVert(I curr, out I newID)
    {
        newID = newDart();
        return insertVertNext(curr, newID);
    }

    [mutating]
    public bool insertNewDartOnEdge(I curr, out I newID)
    {
        newID = newDart();
        return insertEdgeNext(curr, newID);
    }

    [mutating]
    public bool insertNewDartOnFace(I curr, out I newID)
    {
        newID = newDart();
        return insertFaceNext(curr, newID);
    }

    public bool insertVertNext(I curr, I newId)
    {
        I next = model.nextVert(curr);
        vertNext[curr.toInt()] = newId;
        vertNext[newId.toInt()] = next;
        return true;
    }

    public bool insertEdgeNext(I curr, I newId)
    {
        I next = model.nextEdge(curr);
        edgeNext[curr.toInt()] = newId;
        edgeNext[newId.toInt()] = next;
        return true;
    }

    public bool insertFaceNext(I curr, I newId)
    {
        I edgeStep = model.nextEdge(curr);
        I next = model.nextVert(edgeStep);

        // not working
        vertNext[edgeStep.toInt()] = newId;
        vertNext[newId.toInt()] = next;
        return false;
    }

    public bool spliceVertNext(I pos, I other)
    {
        vertNext[pos.toInt()] = other;
        return true;
    }

    public bool spliceEdgeNext(I i)
    {
        I prevDart = prevEdge(i);
        I nextDart = model.nextEdge(i);
        if (prevDart == i && nextDart == i) return false;

        edgeNext[prevDart.toInt()] = nextDart;
        return true;
    }

    public bool spliceFaceNext(I i)
    {
        I prevDart = prevFace(i);
        I nextDart = model.nextVert(model.nextEdge(i));
        if (prevDart == i && nextDart == i) return false;

        // face successor lives on vertNext[edgeNext[prev]]
        I edgeStep = model.nextEdge(prevDart);
        vertNext[edgeStep.toInt()] = nextDart;
        return true;
    }

    public bool atomicInsertVertNext(I curr, I newID)
    {
        while (true)
        {
            I next = vertNextAtomic[curr.toInt()].load();
            if (vertNextAtomic[curr.toInt()].compareExchange(next, newID) == next)
            {
                vertNextAtomic[newID.toInt()].store(next);
                return true;
            }
        }
        return false;
    }

    // if sucess return next dart, or return self dart
    public I atomicRemoveVertNext(I i)
    {
        while (true)
        {
            I prevDart = prevVert(i);
            I nextDart = vertNextAtomic[i.toInt()].load();
            if (prevDart == i && nextDart == i) return i; // single dart orbit

            if (vertNextAtomic[prevDart.toInt()].compareExchange(i, nextDart) == i)
            {
                return nextDart;
            }
        }
        return i;
    }
};
