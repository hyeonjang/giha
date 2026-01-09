#pragma once

#include <giha.h>
#include <giha/sparse.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>


namespace giha {

enum class MeshFormat {
    Auto,
    Obj,
    Ply,
    Stl,
};

template <typename T = float, typename U = uint32_t>
struct PolygonMesh {

    struct Attribute {
        uint32_t components = 0;
        std::vector<T> data;
    };

    SparseVariant<T, U> polygons;
    std::vector<std::array<T, 3>> vertexCoordinates;
    std::unordered_map<std::string, Attribute> vertexAttributes;

    PolygonMesh() = default;
    PolygonMesh(const PolygonMesh& other) { *this = other; }
    PolygonMesh& operator=(const PolygonMesh& other);
    PolygonMesh(PolygonMesh&&) noexcept = default;
    PolygonMesh& operator=(PolygonMesh&&) noexcept = default;

    void clear() {
        polygons.reset();
        vertexCoordinates.clear();
        vertexAttributes.clear();
    }

    static PolygonMesh load(const std::string& path, MeshFormat hint = MeshFormat::Auto);
    void save(const std::string& path, MeshFormat hint = MeshFormat::Auto) const;
};
} // namespace giha
