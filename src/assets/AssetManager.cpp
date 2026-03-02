#include "AssetManager.h"

#include <stb/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/texture.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <functional>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <type_traits>

static std::string normalizeSlashes(std::string p)
{
    for (char& ch : p) if (ch == '\\') ch = '/';
    return p;
}

static std::string resolveTexturePath(const std::string& modelDir, const std::string& texPath)
{
    if (texPath.empty()) return texPath;
    if (texPath[0] == '*') return texPath;                // embedded (*0, *1, ...)
    if (texPath.rfind("data:", 0) == 0) return texPath;   // data URI

    std::filesystem::path p = std::filesystem::path(normalizeSlashes(texPath));
    if (p.is_absolute()) return p.string();

    std::filesystem::path base = std::filesystem::path(modelDir);
    return (base / p).lexically_normal().string();
}

static std::shared_ptr<TextureAsset> uploadTexture2D(
    const unsigned char* pixels, int w, int h, int channels, bool srgb)
{
    if (!pixels || w <= 0 || h <= 0) return nullptr;

    GLenum dataFormat = GL_RGBA;
    GLenum internalFormat = GL_RGBA8;

    if (channels == 1) { dataFormat = GL_RED; internalFormat = GL_R8; }
    else if (channels == 2) { dataFormat = GL_RG; internalFormat = GL_RG8; }
    else if (channels == 3) { dataFormat = GL_RGB; internalFormat = srgb ? GL_SRGB8 : GL_RGB8; }
    else { dataFormat = GL_RGBA; internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8; }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    auto asset = std::make_shared<TextureAsset>();
    asset->id = id;
    asset->width = w;
    asset->height = h;

    const int bpp = (channels == 4 ? 4 : (channels == 3 ? 3 : (channels == 2 ? 2 : 1)));
    asset->approxBytes =
        static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h) * static_cast<std::uint64_t>(bpp);
    return asset;
}

template <typename V>
static auto set_uv(V& v, const glm::vec2& uv, int) -> decltype((void)(v.texCoords = uv), void())
{
    v.texCoords = uv;
}
template <typename V>
static auto set_uv(V& v, const glm::vec2& uv, long) -> decltype((void)(v.texCoord = uv), void())
{
    v.texCoord = uv;
}
template <typename V>
static void set_uv(V&, const glm::vec2&, ...) {}

template <typename V>
static auto set_tangent(V& v, const glm::vec4& t, int) -> decltype((void)(v.tangent = t), void())
{
    v.tangent = t;
}
template <typename V>
static auto set_tangent(V& v, const glm::vec4& t, long) -> decltype((void)(v.tangent = glm::vec3(t)), void())
{
    v.tangent = glm::vec3(t);
}
template <typename V>
static void set_tangent(V&, const glm::vec4&, ...) {}

template <typename V>
static auto set_bitangent(V& v, const glm::vec3& b, int) -> decltype((void)(v.bitangent = b), void())
{
    v.bitangent = b;
}
template <typename V>
static void set_bitangent(V&, const glm::vec3&, ...) {}

static Vertex MakeVertex(const glm::vec3& pos,
    const glm::vec2& uv,
    const glm::vec3& nrm,
    const glm::vec3& tan3,
    const glm::vec3& bit3,
    float tanSign)
{
    Vertex out{};
    out.position = pos;
    out.normal = nrm;
    set_uv(out, uv, 0);

    set_tangent(out, glm::vec4(tan3.x, tan3.y, tan3.z, tanSign), 0);
    set_bitangent(out, bit3, 0);

    return out;
}

AssetManager::AssetManager() {}
AssetManager::~AssetManager() {}

std::shared_ptr<TextureAsset> AssetManager::LoadTexture(const std::string& path, bool srgb)
{
    const std::string key = std::string(srgb ? "srgb:" : "lin:") + path;
    auto it = mTextures.find(key);
    if (it != mTextures.end()) return it->second;

    auto asset = LoadTextureInternal(path, srgb);
    mTextures[key] = asset;
    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::LoadShader(const std::string& vs, const std::string& fs)
{
    std::string key = vs + "+" + fs;
    auto it = mShaders.find(key);
    if (it != mShaders.end()) return it->second;

    auto asset = LoadShaderInternal(vs, fs);
    mShaders[key] = asset;
    return asset;
}

std::shared_ptr<MeshAsset> AssetManager::LoadMesh(const std::string& path, float size)
{
    auto it = mMeshes.find(path);
    if (it != mMeshes.end()) return it->second;

    auto asset = LoadMeshInternal(path, size);
    mMeshes[path] = asset;
    return asset;
}

std::shared_ptr<TextureAsset> AssetManager::GetNullTexture()
{
    static auto nullTex = std::make_shared<TextureAsset>();
    if (!nullTex->id) {
        nullTex->id = GenerateNullTextureGL();
        nullTex->width = nullTex->height = 1;
        nullTex->approxBytes = 4;
    }
    return nullTex;
}

std::shared_ptr<MeshAsset> AssetManager::GetCubeMesh()
{
    static auto cube = std::make_shared<MeshAsset>();
    if (!cube->mesh) {
        cube->mesh = std::make_shared<Mesh>(CreateCubeMeshRaw());
        cube->mesh->SetupMesh();
        const std::uint64_t vb = 8ull * sizeof(Vertex);
        const std::uint64_t ib = 12ull * sizeof(std::uint32_t);
        cube->approxBytes = vb + ib;
    }
    return cube;
}

std::shared_ptr<TextureAsset> AssetManager::LoadTextureInternal(const std::string& filePath, bool srgb)
{
    int w = 0, h = 0, n = 0;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "[AssetManager] Failed to load texture: " << filePath << "\n";
        return GetNullTexture();
    }

    auto asset = uploadTexture2D(data, w, h, n, srgb);
    stbi_image_free(data);

    if (!asset) return GetNullTexture();
    return asset;
}

std::shared_ptr<TextureAsset> AssetManager::LoadEmbeddedTextureInternal(
    const std::string& cacheKey,
    const unsigned char* bytes, int byteCount,
    bool srgb)
{
    const std::string key = std::string(srgb ? "srgb:" : "lin:") + cacheKey;
    auto it = mTextures.find(key);
    if (it != mTextures.end()) return it->second;

    int w = 0, h = 0, n = 0;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load_from_memory(bytes, byteCount, &w, &h, &n, 0);
    if (!data) {
        std::cerr << "[AssetManager] Failed to decode embedded texture: " << cacheKey << "\n";
        auto fallback = GetNullTexture();
        mTextures[key] = fallback;
        return fallback;
    }

    auto asset = uploadTexture2D(data, w, h, n, srgb);
    stbi_image_free(data);

    if (!asset) asset = GetNullTexture();
    mTextures[key] = asset;
    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::LoadShaderInternal(const std::string& vs, const std::string& fs)
{
    auto shaderPtr = std::make_shared<Shader>(vs.c_str(), fs.c_str());
    auto asset = std::make_shared<ShaderAsset>();
    asset->shader = shaderPtr;
    return asset;
}

std::shared_ptr<MeshAsset> AssetManager::LoadMeshInternal(const std::string& modelPath, float desiredSize)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType
    );

    if (!scene || !scene->mRootNode) {
        std::cerr << "[AssetManager] Assimp failed for " << modelPath << ", using cube.\n";
        return GetCubeMesh();
    }

    const std::string modelDir = std::filesystem::path(modelPath).parent_path().string();

    struct TempSubmesh {
        std::vector<Vertex> verts;
        std::vector<std::uint32_t> idx;
        MaterialAsset mat;
    };
    std::vector<TempSubmesh> temps;

    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());

    auto extractMaterial = [&](const aiMaterial* mat) -> MaterialAsset
        {
            MaterialAsset out{};

            aiColor4D diffuse(1, 1, 1, 1);
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                out.baseColorFactor = glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
            }

            aiColor3D emissive(0, 0, 0);
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                out.emissiveFactor = glm::vec3(emissive.r, emissive.g, emissive.b);
            }

            out.metallicFactor = 1.0f;
            out.roughnessFactor = 1.0f;

            auto loadTex = [&](aiTextureType type, bool srgbFlag, std::shared_ptr<TextureAsset>& dst, bool& hasFlag)
                {
                    if (mat->GetTextureCount(type) == 0) return;

                    aiString path;
                    if (mat->GetTexture(type, 0, &path) != AI_SUCCESS) return;

                    std::string p = normalizeSlashes(path.C_Str());
                    if (p.empty()) return;

                    if (!p.empty() && p[0] == '*') {
                        const int id = std::atoi(p.c_str() + 1);
                        if (id >= 0 && id < (int)scene->mNumTextures) {
                            const aiTexture* tex = scene->mTextures[id];
                            if (!tex) return;

                            if (tex->mHeight == 0) {
                                const unsigned char* bytes2 = reinterpret_cast<const unsigned char*>(tex->pcData);
                                const int byteCount2 = (int)tex->mWidth;
                                const std::string cacheKey2 = modelPath + ":*" + std::to_string(id);
                                dst = LoadEmbeddedTextureInternal(cacheKey2, bytes2, byteCount2, srgbFlag);
                                hasFlag = (dst && dst->id != 0);
                                return;
                            }

                            const int w = (int)tex->mWidth;
                            const int h = (int)tex->mHeight;
                            std::vector<unsigned char> rgba((size_t)w * (size_t)h * 4u);

                            for (int i = 0; i < w * h; ++i) {
                                const aiTexel& t = tex->pcData[i];
                                rgba[(size_t)i * 4 + 0] = t.r;
                                rgba[(size_t)i * 4 + 1] = t.g;
                                rgba[(size_t)i * 4 + 2] = t.b;
                                rgba[(size_t)i * 4 + 3] = t.a;
                            }

                            dst = uploadTexture2D(rgba.data(), w, h, 4, srgbFlag);
                            if (!dst) dst = GetNullTexture();
                            hasFlag = (dst && dst->id != 0);
                        }
                        return;
                    }

                    const std::string full = resolveTexturePath(modelDir, p);
                    dst = LoadTexture(full, srgbFlag);
                    hasFlag = (dst && dst->id != 0);
                };

            loadTex(aiTextureType_DIFFUSE, true, out.baseColorMap, out.hasBaseColor);

            loadTex(aiTextureType_NORMALS, false, out.normalMap, out.hasNormal);
            if (!out.hasNormal) loadTex(aiTextureType_HEIGHT, false, out.normalMap, out.hasNormal);

            loadTex(aiTextureType_UNKNOWN, false, out.metallicRoughnessMap, out.hasMetalRough);
            loadTex(aiTextureType_METALNESS, false, out.metallicMap, out.hasMetallic);
            loadTex(aiTextureType_DIFFUSE_ROUGHNESS, false, out.roughnessMap, out.hasRoughness);

            loadTex(aiTextureType_AMBIENT_OCCLUSION, false, out.aoMap, out.hasAO);
            if (!out.hasAO) loadTex(aiTextureType_LIGHTMAP, false, out.aoMap, out.hasAO);

            loadTex(aiTextureType_EMISSIVE, true, out.emissiveMap, out.hasEmissive);

            return out;
        };

    std::function<void(const aiNode*, const aiMatrix4x4&)> walk =
        [&](const aiNode* node, const aiMatrix4x4& parent)
        {
            aiMatrix4x4 global = parent * node->mTransformation;

            aiMatrix3x3 nmat = aiMatrix3x3(global);
            nmat.Inverse().Transpose();

            for (unsigned mi = 0; mi < node->mNumMeshes; ++mi) {
                const aiMesh* m = scene->mMeshes[node->mMeshes[mi]];
                if (!m || m->mNumVertices == 0 || m->mNumFaces == 0) continue;

                TempSubmesh tmp;
                tmp.verts.reserve(m->mNumVertices);

                const bool hasNormals = (m->mNormals != nullptr);
                const bool hasTex = (m->mTextureCoords[0] != nullptr);
                const bool hasTB = m->HasTangentsAndBitangents();

                for (unsigned v = 0; v < m->mNumVertices; ++v) {
                    aiVector3D p = m->mVertices[v];
                    p *= global;
                    glm::vec3 pos(p.x, p.y, p.z);

                    glm::vec3 nor(0.0f, 1.0f, 0.0f);
                    if (hasNormals) {
                        aiVector3D n = m->mNormals[v];
                        n *= nmat;
                        nor = glm::normalize(glm::vec3(n.x, n.y, n.z));
                    }

                    glm::vec2 uv(0.0f, 0.0f);
                    if (hasTex) {
                        const aiVector3D& t = m->mTextureCoords[0][v];
                        uv = glm::vec2(t.x, t.y);
                    }

                    glm::vec3 tan3(1.0f, 0.0f, 0.0f);
                    glm::vec3 bit3(0.0f, 0.0f, 1.0f);
                    float tanSign = 1.0f;

                    if (hasTB) {
                        aiVector3D tA = m->mTangents[v];
                        aiVector3D bA = m->mBitangents[v];

                        tA *= nmat;
                        bA *= nmat;

                        tan3 = glm::normalize(glm::vec3(tA.x, tA.y, tA.z));
                        bit3 = glm::normalize(glm::vec3(bA.x, bA.y, bA.z));

                        tanSign = (glm::dot(glm::cross(nor, tan3), bit3) < 0.0f) ? -1.0f : 1.0f;
                    }
                    else {
                        glm::vec3 up = (std::abs(nor.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                        tan3 = glm::normalize(glm::cross(up, nor));
                        bit3 = glm::normalize(glm::cross(nor, tan3));
                        tanSign = 1.0f;
                    }

                    minB = glm::min(minB, pos);
                    maxB = glm::max(maxB, pos);

                    Vertex vx = MakeVertex(pos, uv, nor, tan3, bit3, tanSign);
                    tmp.verts.push_back(vx);
                }

                tmp.idx.reserve(m->mNumFaces * 3u);
                for (unsigned f = 0; f < m->mNumFaces; ++f) {
                    const aiFace& face = m->mFaces[f];
                    if (face.mNumIndices == 3) {
                        tmp.idx.push_back(static_cast<std::uint32_t>(face.mIndices[0]));
                        tmp.idx.push_back(static_cast<std::uint32_t>(face.mIndices[1]));
                        tmp.idx.push_back(static_cast<std::uint32_t>(face.mIndices[2]));
                    }
                }

                if (tmp.idx.empty() || tmp.verts.empty()) continue;

                const aiMaterial* mat = (m->mMaterialIndex < scene->mNumMaterials)
                    ? scene->mMaterials[m->mMaterialIndex]
                    : nullptr;
                if (mat) tmp.mat = extractMaterial(mat);

                temps.push_back(std::move(tmp));
            }

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                walk(node->mChildren[c], global);
        };

    walk(scene->mRootNode, aiMatrix4x4());

    if (temps.empty()) {
        std::cerr << "[AssetManager] Empty mesh from " << modelPath << ", using cube.\n";
        return GetCubeMesh();
    }

    glm::vec3 center = 0.5f * (minB + maxB);
    glm::vec3 extents = maxB - minB;
    float maxExtent = std::max(extents.x, std::max(extents.y, extents.z));
    float scale = (maxExtent > 0.0f) ? (desiredSize / maxExtent) : 1.0f;

    auto asset = std::make_shared<MeshAsset>();
    asset->submeshes.reserve(temps.size());

    std::uint64_t totalBytes = 0;

    for (auto& t : temps) {
        for (auto& v : t.verts) {
            v.position = (v.position - center) * scale;
        }

        auto meshPtr = std::make_shared<Mesh>(t.verts, t.idx);
        meshPtr->SetupMesh();

        SubmeshAsset sm;
        sm.mesh = meshPtr;
        sm.material = t.mat;
        sm.approxBytes =
            static_cast<std::uint64_t>(t.verts.size()) * sizeof(Vertex) +
            static_cast<std::uint64_t>(t.idx.size()) * sizeof(std::uint32_t);

        totalBytes += sm.approxBytes;
        asset->submeshes.push_back(std::move(sm));
    }

    asset->mesh = asset->submeshes.front().mesh;
    asset->approxBytes = totalBytes;
    return asset;
}

GLuint AssetManager::GenerateNullTextureGL()
{
    unsigned char pink[4] = { 255, 0, 255, 255 };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

Mesh AssetManager::CreateCubeMeshRaw()
{
    std::vector<Vertex> v;
    v.reserve(8);

    auto add = [&](float px, float py, float pz,
        float u, float vv,
        float nx, float ny, float nz)
        {
            glm::vec3 pos(px, py, pz);
            glm::vec2 uv(u, vv);
            glm::vec3 n(nx, ny, nz);

            glm::vec3 up = (std::abs(n.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::vec3 t = glm::normalize(glm::cross(up, n));
            glm::vec3 b = glm::normalize(glm::cross(n, t));

            v.push_back(MakeVertex(pos, uv, n, t, b, 1.0f));
        };
    add(-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    add(0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    add(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    add(-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);

    add(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
    add(0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);
    add(0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f);
    add(-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f);

    std::vector<std::uint32_t> i = { 0u,1u,2u, 2u,3u,0u, 4u,5u,6u, 6u,7u,4u };
    return Mesh(v, i);
}

AssetMemorySummary AssetManager::SummarizeMemory() const
{
    AssetMemorySummary out{};
    for (const auto& kv : mTextures) if (kv.second) out.textures += kv.second->approxBytes;
    for (const auto& kv : mMeshes)   if (kv.second) out.buffers += kv.second->approxBytes;
    return out;
}
