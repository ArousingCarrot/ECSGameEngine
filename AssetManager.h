#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.h"
#include "Mesh.h"

struct TextureAsset {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    std::uint64_t approxBytes = 0;
};

struct ShaderAsset {
    std::shared_ptr<Shader> shader;
};

struct MaterialAsset {
    std::shared_ptr<TextureAsset> baseColorMap;         // sRGB
    std::shared_ptr<TextureAsset> normalMap;            // linear
    std::shared_ptr<TextureAsset> metallicRoughnessMap; // linear (glTF packed: G=roughness, B=metallic)
    std::shared_ptr<TextureAsset> metallicMap;          // linear (R)
    std::shared_ptr<TextureAsset> roughnessMap;         // linear (R)
    std::shared_ptr<TextureAsset> aoMap;                // linear (R)
    std::shared_ptr<TextureAsset> emissiveMap;          // sRGB

    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    bool hasBaseColor = false;
    bool hasNormal = false;
    bool hasMetalRough = false;
    bool hasMetallic = false;
    bool hasRoughness = false;
    bool hasAO = false;
    bool hasEmissive = false;
};

struct SubmeshAsset {
    std::shared_ptr<Mesh> mesh;
    MaterialAsset material;
    std::uint64_t approxBytes = 0;
};

struct MeshAsset {
    std::shared_ptr<Mesh> mesh;
    std::vector<SubmeshAsset> submeshes;
    std::uint64_t approxBytes = 0;
};

struct AssetMemorySummary {
    std::uint64_t textures = 0;
    std::uint64_t buffers = 0;
    std::uint64_t meshes = 0;
    std::uint64_t other = 0;
};

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    std::shared_ptr<TextureAsset> LoadTexture(const std::string& path, bool srgb = true);
    std::shared_ptr<ShaderAsset>  LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    std::shared_ptr<MeshAsset>    LoadMesh(const std::string& modelPath, float desiredSize = 1.0f);

    std::shared_ptr<TextureAsset> GetNullTexture();
    std::shared_ptr<MeshAsset>    GetCubeMesh();

    AssetMemorySummary SummarizeMemory() const;

private:
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> mTextures;
    std::unordered_map<std::string, std::shared_ptr<ShaderAsset>>  mShaders;
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>>    mMeshes;

    std::shared_ptr<TextureAsset> LoadTextureInternal(const std::string& path, bool srgb);
    std::shared_ptr<TextureAsset> LoadEmbeddedTextureInternal(
        const std::string& cacheKey,
        const unsigned char* bytes, int byteCount,
        bool srgb);

    std::shared_ptr<ShaderAsset> LoadShaderInternal(const std::string& vs, const std::string& fs);
    std::shared_ptr<MeshAsset>   LoadMeshInternal(const std::string& path, float size);

    GLuint GenerateNullTextureGL();
    Mesh   CreateCubeMeshRaw();
};
