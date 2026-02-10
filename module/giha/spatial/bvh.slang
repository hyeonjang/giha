implementing giha;

public interface IBVH
{
    void intersect();
    void findClosestQuery();
}

public struct Node<int N>
{

    uint32_t id;
    uint32_t parent;
    Array<uint32_t, N> children;
}

typealias Node2 = Node<2>;
typealias Node4 = Node<4>;
typealias Node8 = Node<8>;

public struct LBVH<PrimT : IPrim> : IBVH
{

    RWStructuredBuffer<PrimT>  prims;
    RWStructuredBuffer<Node2>  nodes;
    RWStructuredBuffer<float4> bounds;

    void intersect() {};
    void findClosestQuery() {};
}
