#ifndef GIHA_GEOMETRY_DART_H
#define GIHA_GEOMETRY_DART_H

#include <giha/vector.h>
#include <giha/sparse.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <string>
#include <algorithm>

#include <iostream>

// 
// dart (halfedge) structure
// 
// next (rotation system) R/rho
// twin (flip) A/alpha
// 
// next,twin,
// dart-vert: U
// dart-edge: T
// dart-face: X
// U A T^T T X
// 
// only need gather (decrease) and scatter (increase).
// 
namespace giha {

template <typename T>
inline std::tuple<int, int, int> findNearbyVertexIndex(T vertex, const std::span<const T>& vertexLoop) {
    const int vertexCount = static_cast<int>(vertexLoop.size());
    int offset = 0;
    for (int i = 0; i < vertexCount; i++) {
        if (vertexLoop[i] == vertex) break;
        offset++;
    }

    return {
        (offset + vertexCount - 1) % vertexCount,
        (offset % vertexCount),
        (offset + 1) % vertexCount
    };
}

template <typename T>
inline std::tuple<T, T, T> findNearbyVertex(T vertex, const std::span<const T>& vertexLoop) {
    auto [i0, i1, i2] = findNearbyVertexIndex(vertex, vertexLoop);
    return { vertexLoop[i0], vertexLoop[i1], vertexLoop[i2] };
}

template <typename T, typename U>
class DartMapFactory {
public:
    DartMapFactory(const SparseVariant<T, U>& FV)
    : faceVertexIncidence(FV) {
        this->nf = sparse::nrow(faceVertexIncidence);
        this->nv = sparse::ncol(faceVertexIncidence);
        this->vertexFaceIncidence = sparse::transpose(FV);
        this->orderCyclicVertexFaceIncidence();
    }

    DartMapFactory(const SparseVariant<T, U>& FV, SparseVariant<T, U>& VF)
    : faceVertexIncidence(FV), vertexFaceIncidence(VF) {
        this->nv = sparse::nrow(vertexFaceIncidence);
        this->nf = sparse::ncol(vertexFaceIncidence);
        this->orderCyclicVertexFaceIncidence();
    }

private:
    // like inner product pattern: VF X FV
    void orderCyclicVertexFaceIncidence() {

        vertexVertexAdjacency = LILSparse<std::vector<U>, U>(nv, nv, true);

        for (int tail = 0; tail < nv; tail++) {

            auto faces = sparse::row(vertexFaceIncidence, tail);

            auto [indices, values] = orderVFCombinatorialJagged(tail, faces);
            vertexVertexAdjacency.indices[tail] = std::move(indices);
            (*vertexVertexAdjacency.values)[tail] = std::move(values);
        }
    }

    auto orderVFCombinatorialJagged(const U tail, const std::span<const U>& faces) -> std::pair<std::vector<U>, std::vector<std::vector<U>>> {
        auto vertexOrdered = orderVFCombinatorial(tail, faces);

        std::pair<std::vector<U>, std::vector<std::vector<U>>> result;
        result.first.reserve(vertexOrdered.size());
        result.second.reserve(vertexOrdered.size());

        for (auto& [vertex, faceIds] : vertexOrdered) {
            result.first.emplace_back(vertex);
            result.second.emplace_back(std::move(faceIds));
        }
        return result;
    }

    auto orderVFCombinatorialFlatten(const U tail, const std::span<const U>& faces) -> std::pair<std::vector<U>, std::vector<U>> {
        auto vertexOrdered = orderVFCombinatorial(tail, faces);

        std::pair<std::vector<U>, std::vector<std::vector<U>>> result;
        result.first.reserve(vertexOrdered.size());
        result.second.reserve(vertexOrdered.size());

        for (auto& [vertex, faceIds] : vertexOrdered) {
            for (auto& faceId : faceIds) {
                result.first.emplace_back(vertex);
                result.second.emplace_back(std::move(faceId));
            } 
        }
        return result;
    }

    // resolve more than 2 incidence case, what the hell.
    auto orderVFCombinatorial(const U tail, const std::span<const U>& faces) -> std::list<std::pair<U, std::vector<U>>> {

        std::list<std::pair<U, std::vector<U>>> vertexIdOrdered;
        for (const auto faceId : faces) {

            const auto& vertexLoop = sparse::row(faceVertexIncidence, faceId);

            auto [prev, curr, next] = findNearbyVertex(tail, vertexLoop);
            
            // find
            const auto iterPrev = std::find_if(vertexIdOrdered.begin(), vertexIdOrdered.end(), 
                [&prev](const std::pair<U, std::vector<U>>& pair) { return pair.first == prev; }
            );
            
            const auto iterNext = std::find_if(vertexIdOrdered.begin(), vertexIdOrdered.end(), 
                [&next](const std::pair<U, std::vector<U>>& pair) { return pair.first == next; }
            );

            // insertion cases
            // 1. there is no insertion
            if (iterPrev == vertexIdOrdered.end() && iterNext == vertexIdOrdered.end()) {
                vertexIdOrdered.insert(vertexIdOrdered.end(), {{prev, std::vector<U>(1, faceId)}, {next, std::vector<U>(1, faceId)}});
            }
            // 2. find prev and attach next after prev
            else if (iterPrev != vertexIdOrdered.end() && iterNext == vertexIdOrdered.end()) {
                iterPrev->second.push_back(faceId);
                vertexIdOrdered.insert(std::next(iterPrev), {next, std::vector<U>(1, faceId)});
            }
            // 3. find next and attach prev before next
            else if (iterPrev == vertexIdOrdered.end() && iterNext != vertexIdOrdered.end()) {
                iterNext->second.push_back(faceId);
                vertexIdOrdered.insert(iterNext, {prev, std::vector<U>(1, faceId)});
            }
            // 4. dealing
            else if (iterPrev != vertexIdOrdered.end() && iterNext != vertexIdOrdered.end()) {
                auto insertPos = std::next(iterPrev);

                if (iterNext != insertPos) {
                    vertexIdOrdered.splice(insertPos, vertexIdOrdered, iterNext);
                    iterPrev->second.push_back(faceId);
                    iterNext->second.push_back(faceId);
                } else {
                    printf("non manifold\n");
                }
            }
        }

        // for (auto& [vertex, faceIds] : vertexIdOrdered) {
        //     printf("%d: (", vertex);
        //     for (auto& faceId : faceIds) {
        //         printf("%d ", faceId);
        //     }
        //     printf(")");
        // }
        // printf("\n");
        // }

        return vertexIdOrdered;
    }

public:
    size_t nv, nf;
    const SparseVariant<T, U>& faceVertexIncidence;
    SparseVariant<T, U> vertexFaceIncidence;
    LILSparse<std::vector<U>, U> vertexVertexAdjacency;
};

//
// hypermap style dart structure
// non-manifold allowing
//
template <typename Id, typename Key>
struct DartMap {

    DartMap(size_t size)
    : vNext(size), eNext(size), vKeys(size) {}

    // denormalization form from vertex adjacency by cyclic order
    template <typename T>
    static DartMap<Id, Key> fromVertexVertexAdjacnecy(
        const LILSparse<T, Id>& vertexVertexAdjacency
    ) {
        return fromVertexVertexAdjacency(sparse::toCSR(vertexVertexAdjacency));
    }

    template <typename T>
    static DartMap<Id, Key> fromVertexVertexAdjacency(
        const LILSparse<std::vector<T>, Id>& vertexVertexAdjacency
    ) {
        return fromVertexVertexAdjacency(sparse::toCSR(vertexVertexAdjacency));
    }

    template <typename T>
    static DartMap<Id, Key> fromVertexVertexAdjacency(
        const CSRSparse<std::vector<T>, Id>& vertexVertexAdjacency
    ) {
        using EdgeKey = tvec2<Id>;

        DartMap<Id, Key> map(vertexVertexAdjacency.nnz);

        // Group darts by an "edge key" (here: undirected pair {tail, head})
        std::unordered_map<EdgeKey, std::vector<Id>, VectorHash<Id, 2>> edgeMap;
        edgeMap.reserve(vertexVertexAdjacency.nnz); // optional, helps perf

        // --- Pass 1: build σ (vNext) from CSR row order, and collect darts into edge buckets
        for (Key tail = 0; tail < static_cast<Key>(vertexVertexAdjacency.m); ++tail) {

            const Id start  = static_cast<Id>(vertexVertexAdjacency.rowPtr[tail]);
            const Id end    = static_cast<Id>(vertexVertexAdjacency.rowPtr[tail + 1]);
            const Id degree = end - start;

            if (degree == 0) continue;

            for (Id curr = start; curr < end; ++curr) {
                Id next = start + ((curr - start + 1) % degree);

                // σ: next dart around the vertex (row order already encodes CCW)
                map.vNext[curr] = next;
                map.vKeys[curr] = tail;
                // map.eNext[curr] 

                // bucket by undirected pair {tail, head}
                const Key head = static_cast<Key>(vertexVertexAdjacency.colIdx[curr]);
                auto mm = std::minmax(tail, head);
                edgeMap[EdgeKey{static_cast<Id>(mm.first), static_cast<Id>(mm.second)}].push_back(curr);
            }
        }

        // --- Pass 2: resolve α (eNext) as a cyclic permutation within each bucket
        for (auto& [edgeKey, dartIds] : edgeMap) {
            const Id k = static_cast<Id>(dartIds.size());
            if (k == 0) continue;

            // Make one cycle per bucket (hyperedge-orbit)
            for (Id i = 0; i < k; ++i) {
                map.eNext[dartIds[i]] = dartIds[(i + 1) % k];
            }
        }
        return std::move(map);
    }

public:
    size_t count() const { return vKeys.size(); }

public:
    std::vector<Key> vKeys; // explicitly map to vertex information

    // vertex-hyperedge incidence
    std::vector<Id> vNext; // \sigma, v-orbit: vertex rotation, CCW by combinatorial
    std::vector<Id> eNext; // \alpha, e-orbit: edge rotation, non ensure geometrical CCW, just cycle
};         

} // namespace giha
#endif // GIHA_GEOMETRY_DART_H
