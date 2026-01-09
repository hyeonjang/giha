implementing giha;

public struct DartBasis<Id: IInteger, Key: IArithmetic>
{
    RWStructuredBuffer<Key> vertKey;
    RWStructuredBuffer<Id>  vertNext;
    RWStructuredBuffer<Id>  edgeNext;

    // only dart operation, not considering projection components
    // atomic
    //
    // [mutating]
    // public I newId() { InterlockedAdd(count, 1); return (I)count; }
    
    // public I loop(I i) { return vNext[dartTwin[i.toInt()].toInt()]; }
    // public I prev(I i) { return vertPrev[i.toInt()]; }

    public Id ensureVertPrev(Id i)
    {
        Id prev = i;
        Id next = vertNext[i.toInt()];
        while (next != i) {
            prev = next;
            next = vertNext[next.toInt()];
        }
        return prev;
    }

    // 
    // atomic modifier
    // 
    // public void removeVertexNext(I i) {

    //     // remove
    //     int i_prev = prev(i).toInt();
    //     vertNext[i_prev] = vertNext[i.toInt()];
    // }

    // public void insertVertexNext(I prev, I i) {
    //     vertNext[prev.toInt()] = i;
    //     vertNext[i.toInt()] = vertNext[prev.toInt()];
    // }

    //
    // local operations: combined operators
    //
    // Delta-complex operator
    //
    // public void flip(I i) {

    //     // remove
    //     removeVertexNext(i);

    //     // insert to next
    //     I i_loop = loop(i);
    //     I i_twin_loop = dartTwin[i_loop.toInt()];
    //     insertVertexNext(i_twin_loop, i);

    //     // key change
    //     dartKeys[i.toInt()] = dartKeys[i_twin_loop.toInt()];
    // }

    // [mutating]
    // public vector<I, 2> insert_on_line(I i0, K i2_vert) {

    //     // 2 new dart
    //     I i1 = newId();
    //     I i2 = newId();

    //     vertNext[i0.toInt()] = i1;
    //     vertNext[i1.toInt()] = i2;

    //     dartKeys[i1.toInt()] = dartKeys[dartTwin[i0.toInt()].toInt()];
    //     dartKeys[i2.toInt()] = i2_vert;

    //     return vector<I, 2>(i1, i2);
    // }

    //
    // element query to drive quantity
    //
    // public vector<I, 2> twin_edge(I i) {
    //     return vector<I, 2>(i, dartTwin[i.toInt()]);
    // }

    // public vector<I, 3> loop_face(I i0) {

    //     I i1 = vertNext[dartTwin[i0.toInt()].toInt()];
    //     I i2 = vertNext[dartTwin[i1.toInt()].toInt()];

    //     return vector<I, 3>(i0, i1, i2);
    // }

    // public bool loopFace(I i0, out vector<I, 3> face) {
        
    //     I i1 = vertNext[dartTwin[i0.toInt()].toInt()];
    //     I i2 = vertNext[dartTwin[i1.toInt()].toInt()];
    //     I i3 = vertNext[dartTwin[i2.toInt()].toInt()];

    //     if (i0 == i3) {
    //         face = vector<I, 3>(i0, i1, i2);
    //         return true;
    //     }
    //     return false;
    // }

    // public vector<K, 2> pairEdge(vector<I, 2> dartIndex) {
    //     return {
    //         dartKeys[dartIndex.x.toInt()],
    //         dartKeys[dartIndex.y.toInt()],
    //     };
    // }

    // public vector<K, 3> pairFace(vector<I, 3> dartIndex) {
    //     return {
    //         dartKeys[dartIndex.x.toInt()], 
    //         dartKeys[dartIndex.y.toInt()], 
    //         dartKeys[dartIndex.z.toInt()],
    //     };
    // }
};
