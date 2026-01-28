#pragma once

#include <giha/geometry/dart.h>
#include <giha/geometry/polygonmesh.h>

#include <memory>
#include <string>
#include <unordered_map>

struct GeometryAssetDatabase {
    template <typename T, typename U>
    const giha::PolygonMesh<T, U>& mesh(const std::string& path, giha::MeshFormat hint = giha::MeshFormat::Auto)
    {
        return *entryFor<T, U>(path, hint).mesh;
    }

    template <typename T, typename U>
    const giha::DartMap<U, U>& dartMap(const std::string& path, giha::MeshFormat hint = giha::MeshFormat::Auto)
    {
        auto& entry = entryFor<T, U>(path, hint);
        if (!entry.dartMap)
        {
            giha::DartMapFactory<T, U> factory(entry.mesh->polygons);
            entry.dartMap = std::make_shared<giha::DartMap<U, U>>(
                giha::DartMap<U, U>::fromVertexVertexAdjacency(factory.vertexVertexAdjacency));
        }
        return *entry.dartMap;
    }

private:
    template <typename T, typename U>
    struct Entry
    {
        std::shared_ptr<giha::PolygonMesh<T, U>> mesh;
        std::shared_ptr<giha::DartMap<U, U>> dartMap;
    };

    template <typename T, typename U>
    static std::unordered_map<std::string, Entry<T, U>>& cache()
    {
        static std::unordered_map<std::string, Entry<T, U>> c;
        return c;
    }

    template <typename T, typename U>
    static std::string makeKey(const std::string& path, giha::MeshFormat hint)
    {
        return path + "#" + std::to_string(static_cast<int>(hint));
    }

    template <typename T, typename U>
    static Entry<T, U>& entryFor(const std::string& path, giha::MeshFormat hint)
    {
        auto key = makeKey<T, U>(path, hint);
        auto& c = cache<T, U>();
        auto [it, inserted] = c.try_emplace(key);
        if (inserted || !it->second.mesh)
        {
            it->second.mesh = std::make_shared<giha::PolygonMesh<T, U>>(giha::PolygonMesh<T, U>::load(path, hint));
            it->second.dartMap.reset();
        }
        return it->second;
    }
};

extern GeometryAssetDatabase gGeometryAssets;
