#include "giha/linalg/sparse.h"
#include "giha/geometry/polygonmesh.h"

#include "giha/geometry/happly.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace giha {
namespace {

template <typename T, typename U>
LILSparse<T, U>& ensureStorageLIL(PolygonMesh<T, U>& mesh) {
    using Storage = LILSparse<T, U>;
    if (auto* lil = std::get_if<Storage>(&mesh.polygons))
        return *lil;

    auto lil = LILSparse<T, U>();
    mesh.polygons = std::move(lil);
    return std::get<LILSparse<T, U>>(mesh.polygons);
}

template <typename T, typename U>
ELLSparse<T, U>& ensureStorageELL(PolygonMesh<T, U>& mesh)
{
    using Storage = ELLSparse<T, U>;
    if (auto* ell = std::get_if<Storage>(&mesh.polygons))
        return *ell;

    auto ell = ELLSparse<T, U>();
    mesh.polygons = std::move(ell);
    return std::get<ELLSparse<T, U>>(mesh.polygons);
}

template <typename T, typename U>
CSRSparse<T, U>& ensureStorageCSR(PolygonMesh<T, U>& mesh) {
    using Storage = CSRSparse<T, U>;

    if (auto* csr = std::get_if<Storage>(&mesh.polygons))
    {
        if (csr->rowPtr.empty())
            csr->rowPtr.push_back(0);
        return *csr;
    }

    auto csr = CSRSparse<T, U>();
    mesh.polygons = std::move(csr);
    return std::get<CSRSparse<T, U>>(mesh.polygons);
}

template <typename T, typename U>
const CSRSparse<T, U>* getStorageCSR(const PolygonMesh<T, U>& mesh)
{
    return std::get_if<CSRSparse<T, U>>(&mesh.polygons);
}

template <typename T, typename U>
const ELLSparse<T, U>* getStorageELL(const PolygonMesh<T, U>& mesh)
{
    return std::get_if<ELLSparse<T, U>>(&mesh.polygons);
}

template <typename T, typename U>
size_t estimateFaceCount(const PolygonMesh<T, U>& mesh)
{
    if (const auto* csr = getStorageCSR(mesh))
    {
        if (csr->rowPtr.size() > 0)
            return csr->rowPtr.size() - 1;
        return 0;
    }
    if (const auto* ell = getStorageELL(mesh))
    {
        if (ell->width == 0)
            return 0;
        return ell->indices.size() / ell->width;
    }
    return 0;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

MeshFormat deduceFormat(const std::string& path, MeshFormat hint)
{
    if (hint != MeshFormat::Auto)
        return hint;

    const std::filesystem::path fsPath(path);
    const auto ext = toLower(fsPath.extension().string());
    if (ext == ".obj")
        return MeshFormat::Obj;
    if (ext == ".ply")
        return MeshFormat::Ply;
    if (ext == ".stl")
        return MeshFormat::Stl;

    throw std::runtime_error("Unsupported mesh format for path: " + path);
}

struct ObjVertexKey
{
    int v = -1;
    int vt = -1;
    int vn = -1;

    bool operator==(const ObjVertexKey& rhs) const = default;
};

struct ObjVertexKeyHash
{
    size_t operator()(const ObjVertexKey& key) const noexcept
    {
        size_t seed = std::hash<int>{}(key.v);
        auto mix = [](size_t& s, size_t value) {
            s ^= value + 0x9e3779b97f4a7c15ULL + (s << 6) + (s >> 2);
        };
        mix(seed, std::hash<int>{}(key.vt));
        mix(seed, std::hash<int>{}(key.vn));
        return seed;
    }
};

int resolveIndex(const std::string& token, size_t count)
{
    if (token.empty())
        return -1;

    int value = std::stoi(token);
    if (value > 0)
    {
        --value;
    }
    else if (value < 0)
    {
        value = static_cast<int>(count) + value;
    }
    else
    {
        throw std::runtime_error("Wavefront OBJ uses 1-based indices; 0 is invalid.");
    }

    if (value < 0 || static_cast<size_t>(value) >= count)
        throw std::runtime_error("OBJ index out of range.");

    return value;
}

template <typename T>
std::array<T, 3> convertVec3(const std::array<double, 3>& value)
{
    return {static_cast<T>(value[0]), static_cast<T>(value[1]), static_cast<T>(value[2])};
}

template <typename T>
std::array<T, 3> computeNormal(const std::array<T, 3>& a, const std::array<T, 3>& b, const std::array<T, 3>& c)
{
    const std::array<T, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const std::array<T, 3> v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};

    std::array<T, 3> normal{
        u[1] * v[2] - u[2] * v[1],
        u[2] * v[0] - u[0] * v[2],
        u[0] * v[1] - u[1] * v[0],
    };

    const double length = std::sqrt(static_cast<double>(normal[0]) * normal[0] +
                                    static_cast<double>(normal[1]) * normal[1] +
                                    static_cast<double>(normal[2]) * normal[2]);
    if (length > 0.0)
    {
        normal[0] = static_cast<T>(normal[0] / length);
        normal[1] = static_cast<T>(normal[1] / length);
        normal[2] = static_cast<T>(normal[2] / length);
    }
    else
    {
        normal = {T(0), T(0), T(0)};
    }

    return normal;
}

template <typename Mesh>
typename Mesh::Attribute* findAttribute(Mesh& mesh, const std::string& name)
{
    auto it = mesh.vertexAttributes.find(name);
    return it == mesh.vertexAttributes.end() ? nullptr : &it->second;
}

template <typename Mesh>
const typename Mesh::Attribute* findAttribute(const Mesh& mesh, const std::string& name)
{
    auto it = mesh.vertexAttributes.find(name);
    return it == mesh.vertexAttributes.end() ? nullptr : &it->second;
}

template <typename Mesh>
typename Mesh::Attribute& ensureAttribute(Mesh& mesh, const std::string& name, uint32_t components)
{
    auto [it, inserted] = mesh.vertexAttributes.try_emplace(name);
    if (inserted)
    {
        it->second.components = components;
    }
    else if (it->second.components != components)
    {
        throw std::runtime_error("Vertex attribute component mismatch for: " + name);
    }
    return it->second;
}

template <typename Attribute>
bool attributeMatches(const Attribute* attr, size_t vertexCount)
{
    if (!attr || attr->components == 0)
        return false;
    const size_t expected = vertexCount * static_cast<size_t>(attr->components);
    return attr->data.size() == expected;
}

template <typename Attribute>
std::array<float, 3> sampleVec3(const Attribute& attr, size_t index)
{
    std::array<float, 3> value{0.f, 0.f, 0.f};
    const size_t base = index * attr.components;
    const uint32_t limit = std::min<uint32_t>(attr.components, 3);
    for (uint32_t c = 0; c < limit; ++c)
        value[c] = attr.data[base + c];
    return value;
}

template <typename Attribute>
std::array<float, 2> sampleVec2(const Attribute& attr, size_t index)
{
    std::array<float, 2> value{0.f, 0.f};
    const size_t base = index * attr.components;
    const uint32_t limit = std::min<uint32_t>(attr.components, 2);
    for (uint32_t c = 0; c < limit; ++c)
        value[c] = attr.data[base + c];
    return value;
}

template <typename T, typename U, typename Float3>
void appendTriangle(PolygonMesh<T, U>& mesh, const Float3& a, const Float3& b, const Float3& c)
{
    auto& ell = ensureStorageELL(mesh);
    using Scalar = T;
    using Index = U;
    const auto pushVertex = [&](const Float3& src) {
        std::array<Scalar, 3> dst{
            static_cast<Scalar>(src[0]),
            static_cast<Scalar>(src[1]),
            static_cast<Scalar>(src[2]),
        };
        mesh.vertexCoordinates.push_back(dst);
        return static_cast<Index>(mesh.vertexCoordinates.size() - 1);
    };

    const Index ia = pushVertex(a);
    const Index ib = pushVertex(b);
    const Index ic = pushVertex(c);

    ell.indices.push_back(ia);
    ell.indices.push_back(ib);
    ell.indices.push_back(ic);
}

template <typename T, typename U, typename Fn>
void forEachFace(const PolygonMesh<T, U>& mesh, Fn&& fn)
{
    if (const auto* ell = getStorageELL(mesh))
    {
        constexpr size_t faceSize = 3;

        const size_t faceCount = ell->indices.size() / faceSize;
        for (size_t i = 0; i < faceCount; ++i)
        {
            fn(ell->indices.data() + i * faceSize, faceSize);
        }
        return;
    }

    if (const auto* csr = getStorageCSR(mesh))
    {
        if (csr->rowPtr.size() < 2)
            return;

        for (size_t i = 0; i + 1 < csr->rowPtr.size(); ++i)
        {
            const size_t start = csr->rowPtr[i];
            const size_t end = csr->rowPtr[i + 1];
            if (end <= start || end > csr->colIdx.size())
                continue;
            fn(csr->colIdx.data() + start, end - start);
        }
    }
}

template <typename T, typename U>
void triangulateFaces(const PolygonMesh<T, U>& mesh, std::vector<std::array<U, 3>>& triangles) {
    triangles.clear();
    forEachFace<T, U>(mesh, [&](const U* verts, size_t count) {
        if (count < 3)
            return;
        for (size_t i = 1; i + 1 < count; ++i)
            triangles.push_back({verts[0], verts[i], verts[i + 1]});
    });
}

template <typename T, typename U>
PolygonMesh<T, U> loadObj(const std::string& path) {

    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("Failed to open OBJ file: " + path);

    std::vector<std::array<T, 3>> positionsRaw;
    std::vector<std::array<T, 3>> normalsRaw;
    std::vector<std::array<T, 2>> texcoordsRaw;

    PolygonMesh<T, U> mesh;
    auto& csr = ensureStorageCSR(mesh);
    auto& pointers = csr.rowPtr;
    auto& indices = csr.colIdx;

    bool normalAttrPrepared = false;
    bool texAttrPrepared = false;

    std::unordered_map<ObjVertexKey, U, ObjVertexKeyHash> remap;

    const auto addVertex = [&](const ObjVertexKey& key) -> U {
        auto [it, inserted] = remap.emplace(key, static_cast<U>(mesh.vertexCoordinates.size()));
        if (!inserted)
            return it->second;

        mesh.vertexCoordinates.push_back(positionsRaw.at(key.v));

        if (!normalsRaw.empty()) {
            auto& attr = ensureAttribute(mesh, "normal", 3);
            if (!normalAttrPrepared) {
                attr.data.reserve(positionsRaw.size() * 3);
                normalAttrPrepared = true;
            }

            std::array<T, 3> value{0.0, 0.0, 0.0};
            if (key.vn >= 0 && static_cast<size_t>(key.vn) < normalsRaw.size())
                value = normalsRaw[key.vn];

            attr.data.push_back(static_cast<T>(value[0]));
            attr.data.push_back(static_cast<T>(value[1]));
            attr.data.push_back(static_cast<T>(value[2]));
        }

        if (!texcoordsRaw.empty()) {
            auto& attr = ensureAttribute(mesh, "texcoord", 2);
            if (!texAttrPrepared) {
                attr.data.reserve(positionsRaw.size() * 2);
                texAttrPrepared = true;
            }

            std::array<T, 2> value{0.0, 0.0};
            if (key.vt >= 0 && static_cast<size_t>(key.vt) < texcoordsRaw.size())
                value = texcoordsRaw[key.vt];
            attr.data.push_back(static_cast<T>(value[0]));
            attr.data.push_back(static_cast<T>(value[1]));
        }

        return it->second;
    };

    bool hasFaces = false;

    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v")
        {
            double x = 0.0, y = 0.0, z = 0.0;
            ss >> x >> y >> z;
            positionsRaw.push_back({static_cast<T>(x), static_cast<T>(y), static_cast<T>(z)});
        }
        else if (tag == "vn")
        {
            double x = 0.0, y = 0.0, z = 0.0;
            ss >> x >> y >> z;
            normalsRaw.push_back({static_cast<T>(x), static_cast<T>(y), static_cast<T>(z)});
        }
        else if (tag == "vt")
        {
            double u = 0.0, v = 0.0;
            ss >> u >> v;
            texcoordsRaw.push_back({static_cast<T>(u), static_cast<T>(v)});
        }
        else if (tag == "f")
        {
            std::vector<U> faceIndices;
            std::string token;
            while (ss >> token)
            {
                std::array<std::string, 3> parts{};
                size_t partIndex = 0;
                std::stringstream tokenStream(token);
                while (partIndex < 3 && std::getline(tokenStream, parts[partIndex], '/'))
                    ++partIndex;

                ObjVertexKey key{};
                key.v = resolveIndex(parts[0], positionsRaw.size());
                key.vt = parts[1].empty() ? -1 : resolveIndex(parts[1], texcoordsRaw.size());
                key.vn = parts[2].empty() ? -1 : resolveIndex(parts[2], normalsRaw.size());

                faceIndices.push_back(addVertex(key));
            }

            if (faceIndices.size() < 3)
                continue;

            hasFaces = true;
            for (U idx : faceIndices)
                indices.push_back(idx);
            pointers.push_back(static_cast<U>(indices.size()));
        }
    }

    // if (!hasFaces)
    //     mesh.polygons.reset();

    return mesh;
}

template <typename T, typename U>
PolygonMesh<T, U> loadPly(const std::string& path) {
    
    happly::PLYData ply(path);
    PolygonMesh<T, U> mesh;
    auto& lil = ensureStorageLIL(mesh);

    auto vertexPositions = ply.getVertexPositions();
    mesh.vertexCoordinates.reserve(vertexPositions.size());
    for (const auto& pos : vertexPositions)
        mesh.vertexCoordinates.push_back(convertVec3<T>(pos));

    if (ply.hasElement("vertex")) {
        auto& vertexElem = ply.getElement("vertex");
        if (vertexElem.hasProperty("nx") && vertexElem.hasProperty("ny") && vertexElem.hasProperty("nz"))
        {
            auto nx = vertexElem.getProperty<float>("nx");
            auto ny = vertexElem.getProperty<float>("ny");
            auto nz = vertexElem.getProperty<float>("nz");

            auto& attr = ensureAttribute(mesh, "normal", 3);
            attr.data.resize(mesh.vertexCoordinates.size() * 3);
            for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
            {
                attr.data[3 * i + 0] = static_cast<T>(nx[i]);
                attr.data[3 * i + 1] = static_cast<T>(ny[i]);
                attr.data[3 * i + 2] = static_cast<T>(nz[i]);
            }
        }

        auto readTexcoords = [&](const char* uName, const char* vName) {
            if (!(vertexElem.hasProperty(uName) && vertexElem.hasProperty(vName)))
                return false;
            auto u = vertexElem.getProperty<float>(uName);
            auto v = vertexElem.getProperty<float>(vName);

            auto& attr = ensureAttribute(mesh, "texcoord", 2);
            attr.data.resize(mesh.vertexCoordinates.size() * 2);
            for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
            {
                attr.data[2 * i + 0] = static_cast<T>(u[i]);
                attr.data[2 * i + 1] = static_cast<T>(v[i]);
            }
            return true;
        };

        if (!readTexcoords("u", "v"))
            readTexcoords("s", "t");
    }

    if (ply.hasElement("face")) {
        lil.indices = std::move(ply.getFaceIndices<U>());
        lil.m = lil.indices.size();
        lil.n = vertexPositions.size();
    }

    return mesh;
}

bool isLikelyBinaryStl(std::ifstream& stream)
{
    stream.seekg(0, std::ios::end);
    const std::streampos fileSize = stream.tellg();
    if (fileSize < 84)
    {
        stream.seekg(0);
        return false;
    }

    stream.seekg(80, std::ios::beg);
    uint32_t triCount = 0;
    stream.read(reinterpret_cast<char*>(&triCount), sizeof(uint32_t));
    const std::streampos expected = 84 + static_cast<std::streampos>(triCount) * 50;
    stream.seekg(0);
    return expected == fileSize;
}

template <typename T, typename U>
PolygonMesh<T, U> loadBinaryStl(std::ifstream& stream)
{
    PolygonMesh<T, U> mesh;
    auto& ell = ensureStorageELL(mesh);

    char header[80];
    stream.read(header, sizeof(header));

    uint32_t triangleCount = 0;
    stream.read(reinterpret_cast<char*>(&triangleCount), sizeof(uint32_t));

    mesh.vertexCoordinates.reserve(static_cast<size_t>(triangleCount) * 3);
    ell.indices.reserve(static_cast<size_t>(triangleCount) * 3);

    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        float normal[3];
        stream.read(reinterpret_cast<char*>(normal), sizeof(float) * 3);

        std::array<float, 3> vertices[3];
        for (int v = 0; v < 3; ++v)
            stream.read(reinterpret_cast<char*>(vertices[v].data()), sizeof(float) * 3);

        uint16_t attributeByteCount = 0;
        stream.read(reinterpret_cast<char*>(&attributeByteCount), sizeof(uint16_t));

        appendTriangle(mesh, vertices[0], vertices[1], vertices[2]);
    }

    return mesh;
}

template <typename T, typename U>
PolygonMesh<T, U> loadAsciiStl(std::ifstream& stream)
{
    PolygonMesh<T, U> mesh;
    ensureStorageELL(mesh);

    std::array<float, 3> vertices[3];
    int vertexCursor = 0;

    std::string token;
    while (stream >> token)
    {
        if (token == "vertex")
        {
            stream >> vertices[vertexCursor][0] >> vertices[vertexCursor][1] >> vertices[vertexCursor][2];
            ++vertexCursor;
            if (vertexCursor == 3)
            {
                appendTriangle(mesh, vertices[0], vertices[1], vertices[2]);
                vertexCursor = 0;
            }
        }
    }

    return mesh;
}

template <typename T, typename U>
PolygonMesh<T, U> loadStl(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Failed to open STL file: " + path);

    const bool binary = isLikelyBinaryStl(stream);
    if (binary)
        return loadBinaryStl<T, U>(stream);

    stream.close();

    std::ifstream asciiStream(path);
    if (!asciiStream)
        throw std::runtime_error("Failed to re-open STL file for ASCII parsing: " + path);
    return loadAsciiStl<T, U>(asciiStream);
}

template <typename T, typename U>
void saveObj(const PolygonMesh<T, U>& mesh, const std::string& path)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out)
        throw std::runtime_error("Failed to open OBJ file for writing: " + path);

    for (const auto& position : mesh.vertexCoordinates)
        out << "v " << position[0] << " " << position[1] << " " << position[2] << "\n";

    const auto* texAttr = findAttribute(mesh, "texcoord");
    const bool hasTexcoords = attributeMatches(texAttr, mesh.vertexCoordinates.size()) && texAttr->components >= 2;
    if (hasTexcoords)
    {
        for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
        {
            const auto uv = sampleVec2(*texAttr, i);
            out << "vt " << uv[0] << " " << uv[1] << "\n";
        }
    }

    const auto* normalAttr = findAttribute(mesh, "normal");
    const bool hasNormals = attributeMatches(normalAttr, mesh.vertexCoordinates.size()) && normalAttr->components >= 3;
    if (hasNormals)
    {
        for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
        {
            const auto n = sampleVec3(*normalAttr, i);
            out << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
        }
    }

    auto emitIndex = [&](uint32_t index) {
        const uint32_t objIndex = index + 1;
        if (hasTexcoords && hasNormals)
            out << objIndex << "/" << objIndex << "/" << objIndex;
        else if (hasTexcoords)
            out << objIndex << "/" << objIndex;
        else if (hasNormals)
            out << objIndex << "//" << objIndex;
        else
            out << objIndex;
    };

    forEachFace(mesh, [&](const uint32_t* verts, size_t count) {
        if (count < 3)
            return;
        out << "f ";
        for (size_t i = 0; i < count; ++i)
        {
            if (i != 0)
                out << " ";
            emitIndex(verts[i]);
        }
        out << "\n";
    });
}

template <typename T, typename U>
void savePly(const PolygonMesh<T, U>& mesh, const std::string& path)
{
    happly::PLYData plyOut;

    std::vector<std::array<double, 3>> vertexPositions(mesh.vertexCoordinates.size());
    for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
    {
        vertexPositions[i][0] = mesh.vertexCoordinates[i][0];
        vertexPositions[i][1] = mesh.vertexCoordinates[i][1];
        vertexPositions[i][2] = mesh.vertexCoordinates[i][2];
    }
    plyOut.addVertexPositions(vertexPositions);

    auto& vertexElement = plyOut.getElement("vertex");

    const auto* normalAttr = findAttribute(mesh, "normal");
    const bool hasNormals = attributeMatches(normalAttr, mesh.vertexCoordinates.size()) && normalAttr->components >= 3;
    if (hasNormals)
    {
        std::vector<float> nx(mesh.vertexCoordinates.size());
        std::vector<float> ny(mesh.vertexCoordinates.size());
        std::vector<float> nz(mesh.vertexCoordinates.size());
        for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
        {
            const auto n = sampleVec3(*normalAttr, i);
            nx[i] = n[0];
            ny[i] = n[1];
            nz[i] = n[2];
        }
        vertexElement.addProperty<float>("nx", nx);
        vertexElement.addProperty<float>("ny", ny);
        vertexElement.addProperty<float>("nz", nz);
    }

    const auto* texAttr = findAttribute(mesh, "texcoord");
    const bool hasTexcoords = attributeMatches(texAttr, mesh.vertexCoordinates.size()) && texAttr->components >= 2;
    if (hasTexcoords)
    {
        std::vector<float> u(mesh.vertexCoordinates.size());
        std::vector<float> v(mesh.vertexCoordinates.size());
        for (size_t i = 0; i < mesh.vertexCoordinates.size(); ++i)
        {
            const auto uv = sampleVec2(*texAttr, i);
            u[i] = uv[0];
            v[i] = uv[1];
        }
        vertexElement.addProperty<float>("u", u);
        vertexElement.addProperty<float>("v", v);
    }

    std::vector<std::vector<U>> facesOut;
    facesOut.reserve(estimateFaceCount(mesh));
    forEachFace(mesh, [&](const uint32_t* verts, size_t count) {
        if (count < 3)
            return;
        facesOut.emplace_back(verts, verts + count);
    });

    if (!facesOut.empty())
        plyOut.addFaceIndices(facesOut);

    plyOut.write(path, happly::DataFormat::Binary);
}

template <typename T, typename U>
void saveStl(const PolygonMesh<T, U>& mesh, const std::string& path)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Failed to open STL file for writing: " + path);

    char header[80];
    std::memset(header, 0, sizeof(header));
    std::memcpy(header, "giha-mesh", 10);
    out.write(header, sizeof(header));

    std::vector<std::array<U, 3>> triangles;
    triangulateFaces(mesh, triangles);
    const uint32_t triangleCount = static_cast<uint32_t>(triangles.size());
    out.write(reinterpret_cast<const char*>(&triangleCount), sizeof(uint32_t));

    const auto* normalAttr = findAttribute(mesh, "normal");
    const bool hasNormals = attributeMatches(normalAttr, mesh.vertexCoordinates.size()) && normalAttr->components >= 3;

    const auto writeVertex = [&](const std::array<T, 3>& v) {
        std::array<float, 3> fv{
            static_cast<float>(v[0]),
            static_cast<float>(v[1]),
            static_cast<float>(v[2]),
        };
        out.write(reinterpret_cast<const char*>(fv.data()), sizeof(float) * 3);
    };

    for (const auto& tri : triangles)
    {
        const auto& a = mesh.vertexCoordinates.at(tri[0]);
        const auto& b = mesh.vertexCoordinates.at(tri[1]);
        const auto& c = mesh.vertexCoordinates.at(tri[2]);

        std::array<float, 3> normal;
        if (hasNormals)
            normal = sampleVec3(*normalAttr, tri[0]);
        else
        {
            const auto computed = computeNormal(a, b, c);
            normal = {static_cast<float>(computed[0]), static_cast<float>(computed[1]), static_cast<float>(computed[2])};
        }

        out.write(reinterpret_cast<const char*>(normal.data()), sizeof(float) * 3);
        writeVertex(a);
        writeVertex(b);
        writeVertex(c);

        const uint16_t attributeByteCount = 0;
        out.write(reinterpret_cast<const char*>(&attributeByteCount), sizeof(uint16_t));
    }
}

template <typename T, typename U>
PolygonMesh<T, U> loadMeshImpl(const std::string& path, MeshFormat hint)
{
    switch (deduceFormat(path, hint))
    {
    case MeshFormat::Obj: return loadObj<T, U>(path);
    case MeshFormat::Ply: return loadPly<T, U>(path);
    case MeshFormat::Stl: return loadStl<T, U>(path);
    default: break;
    }

    throw std::runtime_error("Unsupported mesh format");
}

template <typename T, typename U>
void saveMeshImpl(const PolygonMesh<T, U>& mesh, const std::string& path, MeshFormat hint)
{
    switch (deduceFormat(path, hint))
    {
    case MeshFormat::Obj: saveObj(mesh, path); break;
    case MeshFormat::Ply: savePly(mesh, path); break;
    case MeshFormat::Stl: saveStl(mesh, path); break;
    default: throw std::runtime_error("Unsupported mesh format");
    }
}

} // namespace

template <typename T, typename U>
PolygonMesh<T, U> PolygonMesh<T, U>::load(const std::string& path, MeshFormat hint)
{
    return loadMeshImpl<T, U>(path, hint);
}

template <typename T, typename U>
void PolygonMesh<T, U>::save(const std::string& path, MeshFormat hint) const
{
    saveMeshImpl<T, U>(*this, path, hint);
}

template PolygonMesh<f32, u32> PolygonMesh<f32, u32>::load(const std::string& path, MeshFormat hint);
template PolygonMesh<f64, u32> PolygonMesh<f64, u32>::load(const std::string& path, MeshFormat hint);
template void PolygonMesh<f32, u32>::save(const std::string& path, MeshFormat hint) const;
template void PolygonMesh<f64, u32>::save(const std::string& path, MeshFormat hint) const;

} // namespace giha
