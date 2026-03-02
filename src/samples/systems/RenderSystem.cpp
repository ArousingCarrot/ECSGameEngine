#include "GraphicsGL.h"
#include "RenderSystem.h"
#include "Window.h"
#include "AssetManager.h"
#include "Shader.h"
#include "Mesh.h"
#include "RenderState.h"
#include "AppState.h"
#include "Diagnostics.h"
#include "GpuTimers.h"
#include "RenderDebugOptions.h"
#include "EditorUI.h"
#include "PathTracerGL.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/imgui.h>

#include <iostream>
#include <cstdio>
#include <memory>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

static glm::vec3 g_loadedCenter = glm::vec3(0.0f);
static float     g_loadedRadius = 1.0f;
static bool      g_loadedBoundsValid = false;

static bool IsFiniteMat4(const glm::mat4& m)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

static bool AccumulateMeshBounds(const std::shared_ptr<Mesh>& mesh, glm::vec3& bmin, glm::vec3& bmax)
{
    if (!mesh) return false;
    const auto& v = mesh->GetVertices();
    if (v.empty()) return false;

    for (const Vertex& vert : v)
    {
        bmin = glm::min(bmin, vert.position);
        bmax = glm::max(bmax, vert.position);
    }
    return true;
}

static bool ComputeMeshAssetBounds(const std::shared_ptr<MeshAsset>& asset, glm::vec3& outCenter, float& outRadius)
{
    if (!asset) return false;

    glm::vec3 bmin(std::numeric_limits<float>::max());
    glm::vec3 bmax(-std::numeric_limits<float>::max());
    bool any = false;

    if (asset->mesh) any |= AccumulateMeshBounds(asset->mesh, bmin, bmax);
    for (const auto& sm : asset->submeshes)
        if (sm.mesh) any |= AccumulateMeshBounds(sm.mesh, bmin, bmax);

    if (!any) return false;

    outCenter = 0.5f * (bmin + bmax);
    const glm::vec3 ext = 0.5f * (bmax - bmin);
    outRadius = glm::length(ext);
    if (!std::isfinite(outRadius) || outRadius < 1e-4f) outRadius = 1.0f;

    std::fprintf(stderr, "[RenderSystem] Model bounds: center=(%.3f, %.3f, %.3f) radius=%.3f\n",
        outCenter.x, outCenter.y, outCenter.z, outRadius);

    return true;
}

RenderSystem::RenderSystem(Window* window, AssetManager* assets)
    : mWindow(window), mAssets(assets)
{
}

RenderSystem::~RenderSystem()
{
    destroySceneTarget();
}

void RenderSystem::lazyInit()
{
    if (initialized) return;
    if (!mAssets) {
        std::cerr << "[RenderSystem] Missing AssetManager pointer.\n";
        return;
    }

    auto shAsset = mAssets->LoadShader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");
    if (shAsset && shAsset->shader) shader = shAsset->shader;

    model = mAssets->LoadMesh("Models/1975930Turbo/scene.gltf", 2.0f);
    if (!model || (!model->mesh && model->submeshes.empty())) {
        model = mAssets->GetCubeMesh();
    }

    g_loadedBoundsValid = ComputeMeshAssetBounds(model, g_loadedCenter, g_loadedRadius);

    mNullTex = mAssets->GetNullTexture()->id;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    pt::Initialize();
    pt::GetSettings().enabled = false;

    {
        auto UploadMeshAssetToPathTracer = [&](const std::shared_ptr<MeshAsset>& asset)
            {
                if (!asset) { pt::ClearScene(); return; }

                std::vector<pt::TriInput> tris;
                std::vector<pt::MaterialInput> mats;

                auto addMesh = [&](const std::shared_ptr<Mesh>& mesh, std::uint32_t matIdx)
                    {
                        if (!mesh) return;
                        const auto& v = mesh->GetVertices();
                        const auto& idx = mesh->GetIndices();
                        if (v.empty() || idx.size() < 3) return;

                        tris.reserve(tris.size() + idx.size() / 3);

                        for (size_t i = 0; i + 2 < idx.size(); i += 3)
                        {
                            const Vertex& a = v[idx[i + 0]];
                            const Vertex& b = v[idx[i + 1]];
                            const Vertex& c = v[idx[i + 2]];

                            glm::vec3 faceN = glm::normalize(glm::cross(b.position - a.position, c.position - a.position));
                            auto pickN = [&](const glm::vec3& n) {
                                return (glm::dot(n, n) > 1e-12f) ? glm::normalize(n) : faceN;
                                };

                            pt::TriInput t{};
                            t.v0[0] = a.position.x; t.v0[1] = a.position.y; t.v0[2] = a.position.z;
                            t.v1[0] = b.position.x; t.v1[1] = b.position.y; t.v1[2] = b.position.z;
                            t.v2[0] = c.position.x; t.v2[1] = c.position.y; t.v2[2] = c.position.z;
                            glm::vec3 na = pickN(a.normal);
                            glm::vec3 nb = pickN(b.normal);
                            glm::vec3 nc = pickN(c.normal);
                            t.n0[0] = na.x; t.n0[1] = na.y; t.n0[2] = na.z;
                            t.n1[0] = nb.x; t.n1[1] = nb.y; t.n1[2] = nb.z;
                            t.n2[0] = nc.x; t.n2[1] = nc.y; t.n2[2] = nc.z;
                            t.uv0[0] = a.texCoords.x; t.uv0[1] = a.texCoords.y;
                            t.uv1[0] = b.texCoords.x; t.uv1[1] = b.texCoords.y;
                            t.uv2[0] = c.texCoords.x; t.uv2[1] = c.texCoords.y;


                            t.material = matIdx;
                            tris.push_back(t);
                        }
                    };

                if (!asset->submeshes.empty())
                {
                    mats.resize(asset->submeshes.size());
                    for (size_t i = 0; i < asset->submeshes.size(); ++i)
                    {
                        const auto& m = asset->submeshes[i].material;

                        pt::MaterialInput pm{};
                        pm.baseColor[0] = m.baseColorFactor.r;
                        pm.baseColor[1] = m.baseColorFactor.g;
                        pm.baseColor[2] = m.baseColorFactor.b;
                        pm.baseColor[3] = m.baseColorFactor.a;

                        pm.emissive[0] = m.emissiveFactor.r;
                        pm.emissive[1] = m.emissiveFactor.g;
                        pm.emissive[2] = m.emissiveFactor.b;

                        pm.roughness = m.roughnessFactor;
                        pm.metallic = m.metallicFactor;

                        pm.baseColorTexGL = (m.baseColorMap ? m.baseColorMap->id : 0u);


                        mats[i] = pm;
                        addMesh(asset->submeshes[i].mesh, (std::uint32_t)i);
                    }
                }
                else
                {
                    // Single-mesh fallback: must still provide at least 1 material.
                    mats.resize(1);
                    mats[0].baseColor[0] = 1.0f; mats[0].baseColor[1] = 1.0f; mats[0].baseColor[2] = 1.0f; mats[0].baseColor[3] = 1.0f;
                    mats[0].emissive[0] = 0.0f;  mats[0].emissive[1] = 0.0f;  mats[0].emissive[2] = 0.0f;
                    mats[0].roughness = 1.0f;
                    mats[0].metallic = 0.0f;
                    addMesh(asset->mesh, 0);
                }
                // Compute bounds and publish to the editor so the camera can be framed reliably.
                {
                    glm::vec3 center(0.0f);
                    float radius = 1.0f;

                    if (ComputeMeshAssetBounds(model, center, radius))
                    {
                        std::fprintf(stderr,
                            "[RenderSystem] Model bounds: center=(%.3f, %.3f, %.3f) radius=%.3f\n",
                            center.x, center.y, center.z, radius);

                        const float c[3] = { center.x, center.y, center.z };
                        editor::SetSceneBounds(c, radius);
                        editor::RequestFrame(); // auto-frame once on launch
                    }
                }

                if (!tris.empty() && !mats.empty())
                {
                    std::fprintf(stderr, "[PT] aggregated tris=%zu mats=%zu\n", tris.size(), mats.size());
                    pt::UploadScene(tris.data(), tris.size(), mats.data(), mats.size());
                }
                else
                {
                    pt::ClearScene();
                }
            };

        UploadMeshAssetToPathTracer(model);
    }

    initialized = true;
}

static void bindTexUnit(Shader& sh, int unit, const char* uniformName, GLuint texId)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texId);
    sh.setInt(uniformName, unit);
}

void RenderSystem::destroySceneTarget()
{
    if (mSceneDepthRBO) { glDeleteRenderbuffers(1, &mSceneDepthRBO); mSceneDepthRBO = 0; }
    if (mSceneColorTex) { glDeleteTextures(1, &mSceneColorTex);     mSceneColorTex = 0; }
    if (mSceneFBO) { glDeleteFramebuffers(1, &mSceneFBO);      mSceneFBO = 0; }
    mSceneW = 0;
    mSceneH = 0;
}

bool RenderSystem::ensureSceneTarget(int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (mSceneFBO && mSceneColorTex && mSceneDepthRBO && w == mSceneW && h == mSceneH)
        return true;

    destroySceneTarget();

    glGenFramebuffers(1, &mSceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mSceneFBO);

    glGenTextures(1, &mSceneColorTex);
    glBindTexture(GL_TEXTURE_2D, mSceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSceneColorTex, 0);

    glGenRenderbuffers(1, &mSceneDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, mSceneDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mSceneDepthRBO);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[RenderSystem] Scene FBO incomplete. status=0x" << std::hex << status << std::dec << "\n";
        destroySceneTarget();
        return false;
    }

    mSceneW = w;
    mSceneH = h;
    return true;
}

void RenderSystem::drawModelWithMaterials()
{
    if (!shader || !model) return;

    const auto& dbg = diag::GetRenderDebugOptions();

    // Camera matrices
    glm::mat4 view, proj;

    const bool engineCamOK = RenderState::HasCamera && IsFiniteMat4(RenderState::View) && IsFiniteMat4(RenderState::Projection);
    if (engineCamOK)
    {
        view = RenderState::View;
        proj = RenderState::Projection;
    }
    else
    {
        float aspect = (mSceneH > 0) ? float(mSceneW) / float(mSceneH) : 1.0f;
        const float fovy = glm::radians(45.0f);
        const float tanHalf = std::max(std::tan(fovy * 0.5f), 1e-3f);

        const glm::vec3 target = g_loadedBoundsValid ? g_loadedCenter : glm::vec3(0, 0, 0);
        const float radius = g_loadedBoundsValid ? g_loadedRadius : 1.0f;

        const float dist = (radius / tanHalf) * 1.5f;
        const glm::vec3 eye = target + glm::vec3(0, 0, dist);

        view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));

        const float nearP = std::max(0.01f, dist - radius * 3.0f);
        const float farP = dist + radius * 6.0f;
        proj = glm::perspective(fovy, aspect, nearP, farP);
    }

    glm::mat4 modelM = glm::mat4(1.0f);
    glm::mat4 invV = glm::inverse(view);
    glm::vec3 camPos = glm::vec3(invV[3]);

    shader->use();
    shader->setMat4("model", modelM);
    shader->setMat4("view", view);
    shader->setMat4("projection", proj);

    shader->setVec3("u_CameraPos", camPos);

    // Lighting rig
    shader->setInt("u_LightCount", 3);
    shader->setVec3("u_LightDir[0]", glm::normalize(glm::vec3(0.6f, -1.0f, 0.4f)));
    shader->setVec3("u_LightDir[1]", glm::normalize(glm::vec3(-0.8f, -0.4f, -0.2f)));
    shader->setVec3("u_LightDir[2]", glm::normalize(glm::vec3(0.0f, -0.2f, -1.0f)));

    shader->setVec3("u_LightColor[0]", glm::vec3(1.0f, 0.98f, 0.95f));
    shader->setVec3("u_LightColor[1]", glm::vec3(0.55f, 0.65f, 1.0f));
    shader->setVec3("u_LightColor[2]", glm::vec3(1.0f, 0.6f, 0.25f));

    shader->setFloat("u_LightIntensity[0]", 5.0f);
    shader->setFloat("u_LightIntensity[1]", 1.5f);
    shader->setFloat("u_LightIntensity[2]", 2.0f);

    shader->setVec3("u_AmbientColor", glm::vec3(1.0f));
    shader->setFloat("u_AmbientIntensity", 0.05f);

    shader->setFloat("u_Exposure", 1.1f);
    shader->setFloat("u_Gamma", 2.2f);

    // Sampler units
    shader->setInt("u_BaseColorMap", 0);
    shader->setInt("u_NormalMap", 1);
    shader->setInt("u_MetalRoughMap", 2);
    shader->setInt("u_MetalMap", 3);
    shader->setInt("u_RoughMap", 4);
    shader->setInt("u_AOMap", 5);
    shader->setInt("u_EmissiveMap", 6);

    auto drawOne = [&](const std::shared_ptr<Mesh>& mesh, const MaterialAsset* matOpt)
        {
            // Default material
            MaterialAsset mat{};
            mat.baseColorFactor = glm::vec4(1.0f);
            mat.emissiveFactor = glm::vec3(0.0f);
            mat.metallicFactor = 0.0f;
            mat.roughnessFactor = 1.0f;

            if (matOpt) mat = *matOpt;

            shader->setVec4("u_BaseColorFactor", mat.baseColorFactor);
            shader->setVec3("u_EmissiveFactor", mat.emissiveFactor);
            shader->setFloat("u_MetallicFactor", mat.metallicFactor);
            shader->setFloat("u_RoughnessFactor", mat.roughnessFactor);

            shader->setBool("u_HasBaseColorMap", mat.hasBaseColor && dbg.texturesEnabled);
            shader->setBool("u_HasNormalMap", mat.hasNormal && dbg.texturesEnabled);
            shader->setBool("u_HasMetalRoughMap", mat.hasMetalRough && dbg.texturesEnabled);
            shader->setBool("u_HasMetalMap", mat.hasMetallic && dbg.texturesEnabled);
            shader->setBool("u_HasRoughMap", mat.hasRoughness && dbg.texturesEnabled);
            shader->setBool("u_HasAOMap", mat.hasAO && dbg.texturesEnabled);
            shader->setBool("u_HasEmissiveMap", mat.hasEmissive && dbg.texturesEnabled);

            bindTexUnit(*shader, 0, "u_BaseColorMap", (mat.baseColorMap ? mat.baseColorMap->id : mNullTex));
            bindTexUnit(*shader, 1, "u_NormalMap", (mat.normalMap ? mat.normalMap->id : mNullTex));
            bindTexUnit(*shader, 2, "u_MetalRoughMap", (mat.metallicRoughnessMap ? mat.metallicRoughnessMap->id : mNullTex));
            bindTexUnit(*shader, 3, "u_MetalMap", (mat.metallicMap ? mat.metallicMap->id : mNullTex));
            bindTexUnit(*shader, 4, "u_RoughMap", (mat.roughnessMap ? mat.roughnessMap->id : mNullTex));
            bindTexUnit(*shader, 5, "u_AOMap", (mat.aoMap ? mat.aoMap->id : mNullTex));
            bindTexUnit(*shader, 6, "u_EmissiveMap", (mat.emissiveMap ? mat.emissiveMap->id : mNullTex));

            if (mesh) mesh->Draw();
        };

    if (!model->submeshes.empty()) {
        for (const auto& sm : model->submeshes) {
            drawOne(sm.mesh, &sm.material);
        }
    }
    else {
        drawOne(model->mesh, nullptr);
    }
}

void RenderSystem::Update(float dt)
{
    (void)dt;

    if (!mWindow || !mWindow->isInitialized()) return;
    if (!initialized) lazyInit();

    // Scene viewport size comes from EditorUI.
    auto sv = editor::GetSceneViewportInfo();
    int sceneW = sv.pixelW;
    int sceneH = sv.pixelH;
    if (sceneW <= 0 || sceneH <= 0) {
        SDL_GetWindowSizeInPixels(mWindow->getSDLWindow(), &sceneW, &sceneH);
    }
    if (sceneW < 1) sceneW = 1;
    if (sceneH < 1) sceneH = 1;

    // Path Tracer output routing
    if (pt::GetSettings().enabled)
    {
        diag::ScopedGpuZone gpuPtScope("PathTracer");

        // Drive PT camera from engine camera so rays actually hit the uploaded scene.
        {
            glm::mat4 view, proj;
            if (RenderState::HasCamera) {
                view = RenderState::View;
                proj = RenderState::Projection;
            }
            else {
                view = glm::lookAt(glm::vec3(0, 0, 6), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
                float aspect = (sceneH > 0) ? float(sceneW) / float(sceneH) : 1.0f;
                proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);
            }

            glm::mat4 invV = glm::inverse(view);
            glm::vec3 pos = glm::vec3(invV[3]);
            glm::vec3 right = glm::normalize(glm::vec3(invV[0]));
            glm::vec3 up = glm::normalize(glm::vec3(invV[1]));
            glm::vec3 fwd = -glm::normalize(glm::vec3(invV[2]));

            float tanHalfFovY = 1.0f;
            if (proj[1][1] != 0.0f) tanHalfFovY = 1.0f / proj[1][1];

            float p[3]{ pos.x, pos.y, pos.z };
            float d[3]{ fwd.x, fwd.y, fwd.z };
            float r[3]{ right.x, right.y, right.z };
            float u[3]{ up.x, up.y, up.z };
            pt::SetCameraBasis(p, d, r, u, tanHalfFovY);
        }

        pt::Render(sceneW, sceneH);

        const std::uint32_t outTex = pt::GetOutputTextureGL();
        if (outTex != 0)
        {
            editor::SetSceneTexture((std::uint64_t)outTex, /*flipY=*/false);
            return;
        }
        else
        {
            std::cerr << "[RenderSystem] PT enabled but output texture is 0; falling back to raster this frame.\n";
        }
    }

    // Raster scene path (default)
    const auto& dbg = diag::GetRenderDebugOptions();

    if (!ensureSceneTarget(sceneW, sceneH)) {
        editor::SetSceneTexture((std::uint64_t)0);
        return;
    }

    diag::ScopedGpuZone gpuSceneScope("ScenePass");

    glBindFramebuffer(GL_FRAMEBUFFER, mSceneFBO);
    glViewport(0, 0, mSceneW, mSceneH);

    // Avoid dark-pane alpha blending: render alpha=1 into the scene texture.
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    if (dbg.disableCulling) glDisable(GL_CULL_FACE);
    else { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW); }

    glPolygonMode(GL_FRONT_AND_BACK, dbg.wireframe ? GL_LINE : GL_FILL);

    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawModelWithMaterials();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    editor::SetSceneTexture((std::uint64_t)mSceneColorTex, /*flipY=*/true);
}
