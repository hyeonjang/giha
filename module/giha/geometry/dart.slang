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

    __init(StructuredBuffer<I> _vertNext)
    {
        vertNext = _vertNext;
    }

    StructuredBuffer<I> vertNext;
}

struct EdgeNext<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = VERT_ORBIT_MAX_STEPS;
    I next(I i) { return edgeNext[i.toInt()]; }

    __init(StructuredBuffer<I> _edgeNext)
    {
        edgeNext = _edgeNext;
    }

    StructuredBuffer<I> edgeNext;
}

struct FaceNext<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = FACE_ORBIT_MAX_STEPS;
    I next(I i) { return vertNext[edgeNext[i.toInt()].toInt()]; }

    __init(StructuredBuffer<I> _vertNext, StructuredBuffer<I> _edgeNext)
    {
        vertNext = _vertNext;
        edgeNext = _edgeNext;
    }

    StructuredBuffer<I> vertNext;
    StructuredBuffer<I> edgeNext;
}

// RW
struct VertNextRW<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = VERT_ORBIT_MAX_STEPS;
    I next(I i) { return vertNext[i.toInt()]; }

    __init(RWStructuredBuffer<I> _vertNext) { vertNext = _vertNext; }
    RWStructuredBuffer<I> vertNext;
}

struct EdgeNextRW<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = VERT_ORBIT_MAX_STEPS;
    I next(I i) { return edgeNext[i.toInt()]; }

    __init(RWStructuredBuffer<I> _edgeNext) { edgeNext = _edgeNext; }
    RWStructuredBuffer<I> edgeNext;
}

struct FaceNextRW<I : IInteger> : INextOP<I>
{
    static const uint MaxSteps = FACE_ORBIT_MAX_STEPS;
    I next(I i) { return vertNext[edgeNext[i.toInt()].toInt()]; }

    __init(RWStructuredBuffer<I> _vertNext, RWStructuredBuffer<I> _edgeNext)
    {
        vertNext = _vertNext;
        edgeNext = _edgeNext;
    }

    RWStructuredBuffer<I> vertNext;
    RWStructuredBuffer<I> edgeNext;
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

public struct SigmaModel<I: IInteger, K: IArithmetic>
{
    StructuredBuffer<uint> counter;
    StructuredBuffer<I> vertNext; // \sigma, v-orbit: vertex rotation, CCW by combinatorial
    StructuredBuffer<I> edgeNext; // \alpha, e-orbit: edge rotation, CCW
    StructuredBuffer<K> extrinsicKey;

    // map to extrinsic
    public K mapToExtrinsic(I i0) { return extrinsicKey[i0.toInt()]; }
    public uint count() { return counter[0]; }

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
        return walkOrbit(i0, FaceNext<I>(vertNext, edgeNext), handler);
    }

    public CollectHandler<I> collectVertOrbitIfMin(
        I dartID,
        uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices)
    {
        return collectOrbitIfRepresentative(VertNext<I>(vertNext), FACE_ORBIT_MAX_STEPS,
            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

    public CollectHandler<I> collectEdgeOrbitIfMin(
        I dartID,
        uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices)
    {
        return collectOrbitIfRepresentative(EdgeNext<I>(edgeNext), EDGE_ORBIT_MAX_STEPS,
            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

    public CollectHandler<I> collectFaceOrbitIfMin(
        I dartID,
        uint3 tid,
        RWStructuredBuffer<Atomic<uint>> orbitCounter,
        RWStructuredBuffer<Atomic<uint>> offsetCounter,
        RWStructuredBuffer<uint> indices)
    {
        return collectOrbitIfRepresentative(FaceNext<I>(vertNext, edgeNext), FACE_ORBIT_MAX_STEPS,
            dartID, tid, orbitCounter, offsetCounter, indices
        );
    }

// 
// private
// 
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

public struct MutableSigmaModel<I : IInteger, K : IArithmetic>
{
    RWStructuredBuffer<Atomic<uint>> counter;
    public RWStructuredBuffer<I> vertNext;
    public RWStructuredBuffer<I> edgeNext;
    RWStructuredBuffer<K> extrinsicKey;

    // map
    public K mapToExtrinsic(I i0) { return extrinsicKey[i0.toInt()]; }
    public void assignExtrinsic(I i, K k) { extrinsicKey[i.toInt()] = k; }
    public uint count() { return counter[0].load(); }

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
        return prev(VertNextRW<I>(vertNext), i0);
    };

    public I prevEdge(I i0)
    {
        return prev(EdgeNextRW<I>(edgeNext), i0);
    };

    public I prevFace(I i0)
    {
        return prev(FaceNextRW<I>(vertNext, edgeNext), i0);
    }

    //
    // atomic modifiers
    //
    [mutating]
    public I newDart()
    {
        let inc = counter[0].increment();
        let newID = I(inc);

        // self-rotation
        vertNext[inc] = I(inc);
        edgeNext[inc] = I(inc);
        return newID;
    }

    // 
    public bool insertVertNext(I curr, I newId)
    {
        I next = vertNext[curr.toInt()];
        vertNext[curr.toInt()] = newId;
        vertNext[newId.toInt()] = next;
        return true;
    }

    public bool insertEdgeNext(I curr, I newId)
    {
        I next = edgeNext[curr.toInt()];
        edgeNext[curr.toInt()] = newId;
        edgeNext[newId.toInt()] = next;
        return true;
    }

    public bool insertFaceNext(I curr, I newId)
    {
        I edgeStep = edgeNext[curr.toInt()];
        I next = vertNext[edgeStep.toInt()];

        // not working
        vertNext[edgeStep.toInt()] = newId;
        vertNext[newId.toInt()] = next;
        return false;
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
    // public I insertNew

    public bool spliceVertNext(I i)
    {
        I prevDart = prevVert(i);
        I nextDart = vertNext[i.toInt()];
        if (prevDart == i && nextDart == i) return false; // single dart orbit

        vertNext[prevDart.toInt()] = nextDart;
        return true;
    }

    public bool spliceEdgeNext(I i)
    {
        I prevDart = prevEdge(i);
        I nextDart = edgeNext[i.toInt()];
        if (prevDart == i && nextDart == i) return false;

        edgeNext[prevDart.toInt()] = nextDart;
        return true;
    }

    public bool spliceFaceNext(I i)
    {
        I prevDart = prevFace(i);
        I nextDart = vertNext[edgeNext[i.toInt()].toInt()];
        if (prevDart == i && nextDart == i) return false;

        // face successor lives on vertNext[edgeNext[prev]]
        I edgeStep = edgeNext[prevDart.toInt()];
        vertNext[edgeStep.toInt()] = nextDart;
        return true;
    }


    //
    // local operations: combined operators
    //
    // public void flip(I i)
    // {
    //     // target
    //     I invLoop = edgeNext[vertNext[i.toInt()].toInt()];
    //     vertKeys[i.toInt()] = vertKeys[invLoop.toInt()];

    //     spliceVertNext(i);
    //     insertVertNext(invLoop, i);
    // }

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
    // public vector<I, 2> twin_edge(I i) {
    //     return vector<I, 2>(i, dartTwin[i.toInt()]);
    // }
};
