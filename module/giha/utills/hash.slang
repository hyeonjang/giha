// implementing giha;

// static uint hashMix(uint seed, uint value)
// {
//     value += 0x9e3779b9u;
//     value += seed << 6;
//     value += seed >> 2;
//     return seed ^ value;
// }

// static uint finalizeHash(uint seed)
// {
//     seed ^= seed >> 16;
//     seed *= 0x7feb352du;
//     seed ^= seed >> 15;
//     seed *= 0x846ca68bu;
//     seed ^= seed >> 16;
//     return seed;
// }

// static uint32_t fmix64(uint64_t k)
// {
//     k ^= k >> 33;
//     k *= 0xff51afd7ed558ccdUL;
//     k ^= k >> 33;
//     k *= 0xc4ceb9fe1a85ec53UL;
//     k ^= k >> 33;
//     return uint32_t(k);
// }

// interface IHasher<K>
// {
//     uint hash(K key);
// }

// public struct VectorHasher<I : IInteger, int N> : IHasher<vector<I, N>>
// {
//     public uint hash(vector<I, N> key)
//     {
//         uint64_t acc = 0x9e3779b97f4a7c15UL;
//         [unroll] for (int i = 0; i < N; ++i)
//         {
//             uint64_t v = key[i].toUInt();
//             acc ^= v + 0x9e3779b97f4a7c15UL + (acc << 6) + (acc >> 2); // WYHASH-style mix
//         }
//         return fmix64(acc);
//     }
// }

// public typealias Vector2iHasher = VectorHasher<int, 2>;
// public typealias Vector3iHasher = VectorHasher<int, 3>;
// public typealias Vector4iHasher = VectorHasher<int, 4>;

// // 
// public enum HashState
// {
//     Failed = -2,
//     EMPTY = -1,
//     WRITING  = 1,
//     FULL = 2,
//     TOMB = 3
// }

// public struct HashSet<Key> 
//     where Key : IComparable
// {
//     public HashState insert(Key key)
//     {
//         uint bufferSize = buffer.getCount();
//         uint slot = hasher.hash(key) % bufferSize;

//         int prev;
//         InterlockedCompareExchange(buffer[slot], (int)HashState.EMPTY, (int)HashState.WRITING, prev);
//         if (prev == (int)HashState.EMPTY)
//         {
//             return (HashState)prev;
//         }
//         else if (prev == (int)HashState.WRITING)
//         {
//             return (HashState)prev;
//         }

//         return HashState.Failed;
//     }

//     RWStructuredBuffer<int> buffer;
//     IHasher<Key> hasher;
// }

// // map
// public struct HashMap<K, V, Hasher>
//     where K : IComparable
//     where V : IArithmetic
//     where Hasher : IHasher<K>
// {
//     public struct KV
//     {
//         public int32_t state;
//         // K key;
//     };

//     public Hasher hasher;
//     public RWStructuredBuffer<KV> buffer;

//     public HashState insert(K key, V value)
//     {
//         uint bufferSize = buffer.getCount();
//         uint slot = hasher.hash(key) % bufferSize;
 
//         return HashState.Failed;
//     }
// };
