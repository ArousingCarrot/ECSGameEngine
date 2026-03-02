#include "PathTracerGL.h"

#include <glad/glad.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>

namespace
{
    struct GLProgram
    {
        GLuint id = 0;
        void destroy() { if (id) glDeleteProgram(id); id = 0; }
    };

    static GLuint CompileCompute(const char* src, const char* label)
    {
        GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            GLint len = 0;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len > 1 ? len : 1);
            glGetShaderInfoLog(sh, (GLsizei)log.size(), nullptr, log.data());
            std::fprintf(stderr, "[PathTracerGL] Compute compile failed (%s):\n%s\n", label, log.data());
        }
        return sh;
    }

    static GLuint LinkProgram(GLuint cs, const char* label)
    {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, cs);
        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len > 1 ? len : 1);
            glGetProgramInfoLog(prog, (GLsizei)log.size(), nullptr, log.data());
            std::fprintf(stderr, "[PathTracerGL] Program link failed (%s):\n%s\n", label, log.data());
        }

        glDetachShader(prog, cs);
        return prog;
    }

    static GLProgram MakeComputeProgram(const char* src, const char* label)
    {
        GLProgram p{};
        GLuint cs = CompileCompute(src, label);
        p.id = LinkProgram(cs, label);
        glDeleteShader(cs);
        return p;
    }

    static const char* GLErrorName(GLenum e)
    {
        switch (e)
        {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "GL_UNKNOWN_ERROR";
        }
    }

    static bool DrainGLErrors(const char* where)
    {
        bool had = false;
        for (int i = 0; i < 16; ++i)
        {
            GLenum e = glGetError();
            if (e == GL_NO_ERROR) break;
            had = true;
            std::fprintf(stderr, "[PT][GL] %s: %s (0x%04X)\n", where, GLErrorName(e), (unsigned)e);
        }
        return had;
    }

    static bool IsTexValid(GLuint tex)
    {
        return tex != 0 && glIsTexture(tex) == GL_TRUE;
    }

    static bool IsTexLevelDefined2D(GLuint tex)
    {
        if (!IsTexValid(tex)) return false;

        GLint w = 0, h = 0;
        if (GLAD_GL_VERSION_4_5)
        {
            glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &w);
            glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_HEIGHT, &h);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, tex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        return (w > 0) && (h > 0);
    }

    static GLuint CreateTex2D(int w, int h, GLenum internalFmt, GLenum fmt, GLenum type, bool linearFilter, const char* debugName)
    {
        if (w < 1 || h < 1)
        {
            std::fprintf(stderr, "[PT][Res] CreateTex2D(%s) invalid size %dx%d (refusing)\n",
                debugName ? debugName : "unnamed", w, h);
            return 0;
        }
        DrainGLErrors("CreateTex2D(pre)");

        GLuint tex = 0;

        if (GLAD_GL_VERSION_4_5)
        {
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            if (!IsTexValid(tex))
            {
                std::fprintf(stderr, "[PT][Res] glCreateTextures failed for %s\n", debugName ? debugName : "unnamed");
                DrainGLErrors("glCreateTextures");
                return 0;
            }

            glTextureStorage2D(tex, 1, internalFmt, w, h);
            glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
            glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glClearTexImage(tex, 0, fmt, type, nullptr);
        }
        else
        {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFmt, w, h);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glClearTexImage(tex, 0, fmt, type, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        if (DrainGLErrors(debugName ? debugName : "CreateTex2D(post)") || !IsTexLevelDefined2D(tex))
        {
            std::fprintf(stderr, "[PT][Res] Allocation failed for %s; deleting tex=%u\n",
                debugName ? debugName : "unnamed", (unsigned)tex);
            if (tex) glDeleteTextures(1, &tex);
            return 0;
        }

        std::fprintf(stderr, "[PT][Res] %s: tex=%u %dx%d internalFmt=0x%X\n",
            debugName ? debugName : "Tex2D", (unsigned)tex, w, h, (unsigned)internalFmt);

        return tex;
    }

    static void DestroyTex(GLuint& tex)
    {
        if (tex) glDeleteTextures(1, &tex);
        tex = 0;
    }

    static GLuint CreateSSBO(const void* data, std::size_t bytes, GLenum usage)
    {
        GLuint buf = 0;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, data, usage);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return buf;
    }

    static void DestroySSBO(GLuint& buf)
    {
        if (buf) glDeleteBuffers(1, &buf);
        buf = 0;
    }

    static GLuint CreateUBO(std::size_t bytes, GLenum usage)
    {
        GLuint buf = 0;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_UNIFORM_BUFFER, buf);
        glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)bytes, nullptr, usage);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return buf;
    }

    static void UpdateUBO(GLuint buf, const void* data, std::size_t bytes)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, buf);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, (GLsizeiptr)bytes, data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    struct GpuPassTimer
    {
        static constexpr int kFrames = 4;

        std::array<GLuint, kFrames> qBegin{};
        std::array<GLuint, kFrames> qEnd{};

        float lastMs = 0.0f;

        bool valid = false;
        bool warned = false;

        void init()
        {
            shutdown();
            if (GLAD_GL_VERSION_4_5)
            {
                glCreateQueries(GL_TIMESTAMP, kFrames, qBegin.data());
                glCreateQueries(GL_TIMESTAMP, kFrames, qEnd.data());
            }
            else
            {
                glGenQueries(kFrames, qBegin.data());
                glGenQueries(kFrames, qEnd.data());
            }

            valid = true;
            for (int i = 0; i < kFrames; ++i)
            {
                if (qBegin[i] == 0 || qEnd[i] == 0 ||
                    glIsQuery(qBegin[i]) == GL_FALSE ||
                    glIsQuery(qEnd[i]) == GL_FALSE)
                {
                    valid = false;
                    break;
                }
            }

            if (!valid && !warned)
            {
                std::fprintf(stderr, "[PathTracerGL] GPU timer queries invalid; disabling PT per-pass timings.\n");
                warned = true;
            }
        }

        void shutdown()
        {
            if (qBegin[0] || qEnd[0])
            {
                glDeleteQueries(kFrames, qBegin.data());
                glDeleteQueries(kFrames, qEnd.data());
            }
            qBegin.fill(0);
            qEnd.fill(0);
            valid = false;
        }

        void begin(int frameIndex)
        {
            if (!valid) return;

            const GLuint q = qBegin[frameIndex % kFrames];
            if (q == 0 || glIsQuery(q) == GL_FALSE)
            {
                valid = false;
                if (!warned)
                {
                    std::fprintf(stderr, "[PathTracerGL] Timer query became invalid; disabling PT per-pass timings.\n");
                    warned = true;
                }
                return;
            }

            glQueryCounter(q, GL_TIMESTAMP);
        }

        void end(int frameIndex)
        {
            if (!valid) return;

            const GLuint q = qEnd[frameIndex % kFrames];
            if (q == 0 || glIsQuery(q) == GL_FALSE)
            {
                valid = false;
                if (!warned)
                {
                    std::fprintf(stderr, "[PathTracerGL] Timer query became invalid; disabling PT per-pass timings.\n");
                    warned = true;
                }
                return;
            }

            glQueryCounter(q, GL_TIMESTAMP);
        }

        void resolve(int frameIndex)
        {
            if (!valid) return;

            const int idx = (frameIndex - 2 + kFrames * 1000) % kFrames;

            if (qBegin[idx] == 0 || qEnd[idx] == 0) return;

            GLuint available = 0;
            glGetQueryObjectuiv(qEnd[idx], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return;

            GLuint64 t0 = 0, t1 = 0;
            glGetQueryObjectui64v(qBegin[idx], GL_QUERY_RESULT, &t0);
            glGetQueryObjectui64v(qEnd[idx], GL_QUERY_RESULT, &t1);

            if (t1 > t0)
            {
                const double ns = double(t1 - t0);
                lastMs = float(ns / 1.0e6);
            }
        }
    };

    enum class DebugView : int
    {
        FinalTonemapped = 0,
        AccumHDR = 1,
        SampleHDR = 2,
        Albedo = 3,
        Normal = 4,
        Depth = 5,
        DenoisedHDR = 6
    };

    struct TriGPU
    {
        float v0[4];
        float e1[4];
        float e2[4];
        float n0[4];
        float n1[4];
        float n2[4];

        // uv0.xy, uv1.xy
        float uv01[4];

        // uv2.xy, pad
        float uv2[4];
    };

    struct MaterialGPU
    {
        float baseColor[4];
        float emissiveRough[4]; // emissive.xyz, roughness
        float metallicPad[4];   // metallic, pad, pad, pad

        // Texture indices into the bound sampler arrays. -1 means “no texture”.
        int tex[4]; // baseColor, normal, metalRough, emissive
    };

    struct NodeGPU
    {
        float bmin[4];
        float bmax[4];
        std::uint32_t meta[4]; // left, right, first, count (count>0 => leaf)
    };

    struct AABB
    {
        float mn[3];
        float mx[3];
    };

    static AABB aabbEmpty()
    {
        AABB b{};
        b.mn[0] = b.mn[1] = b.mn[2] = std::numeric_limits<float>::infinity();
        b.mx[0] = b.mx[1] = b.mx[2] = -std::numeric_limits<float>::infinity();
        return b;
    }

    static void aabbGrow(AABB& b, const float p[3])
    {
        b.mn[0] = std::min(b.mn[0], p[0]); b.mn[1] = std::min(b.mn[1], p[1]); b.mn[2] = std::min(b.mn[2], p[2]);
        b.mx[0] = std::max(b.mx[0], p[0]); b.mx[1] = std::max(b.mx[1], p[1]); b.mx[2] = std::max(b.mx[2], p[2]);
    }

    static void aabbGrow(AABB& b, const AABB& o)
    {
        b.mn[0] = std::min(b.mn[0], o.mn[0]); b.mn[1] = std::min(b.mn[1], o.mn[1]); b.mn[2] = std::min(b.mn[2], o.mn[2]);
        b.mx[0] = std::max(b.mx[0], o.mx[0]); b.mx[1] = std::max(b.mx[1], o.mx[1]); b.mx[2] = std::max(b.mx[2], o.mx[2]);
    }

    static float aabbExtent(const AABB& b, int axis) { return b.mx[axis] - b.mn[axis]; }

    struct TriBounds
    {
        AABB bounds;
        float centroid[3];
    };

    struct BuildContext
    {
        std::vector<std::uint32_t>* triIndices = nullptr;
        std::vector<TriBounds> triInfo;
        std::vector<NodeGPU> nodes;

        static constexpr std::uint32_t kLeafMax = 8;

        AABB boundsForRange(std::uint32_t first, std::uint32_t count)
        {
            AABB b = aabbEmpty();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const std::uint32_t ti = (*triIndices)[first + i];
                aabbGrow(b, triInfo[ti].bounds);
            }
            return b;
        }

        AABB centroidBoundsForRange(std::uint32_t first, std::uint32_t count)
        {
            AABB b = aabbEmpty();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const std::uint32_t ti = (*triIndices)[first + i];
                aabbGrow(b, triInfo[ti].centroid);
            }
            return b;
        }

        std::uint32_t buildNode(std::uint32_t first, std::uint32_t count)
        {
            const std::uint32_t nodeIdx = (std::uint32_t)nodes.size();
            nodes.push_back(NodeGPU{});

            AABB b = boundsForRange(first, count);

            nodes[nodeIdx].bmin[0] = b.mn[0];
            nodes[nodeIdx].bmin[1] = b.mn[1];
            nodes[nodeIdx].bmin[2] = b.mn[2];
            nodes[nodeIdx].bmin[3] = 0.0f;

            nodes[nodeIdx].bmax[0] = b.mx[0];
            nodes[nodeIdx].bmax[1] = b.mx[1];
            nodes[nodeIdx].bmax[2] = b.mx[2];
            nodes[nodeIdx].bmax[3] = 0.0f;

            if (count <= kLeafMax)
            {
                nodes[nodeIdx].meta[0] = 0;
                nodes[nodeIdx].meta[1] = 0;
                nodes[nodeIdx].meta[2] = first;
                nodes[nodeIdx].meta[3] = count;
                return nodeIdx;
            }

            AABB cb = centroidBoundsForRange(first, count);
            int axis = 0;
            float ex = aabbExtent(cb, 0);
            float ey = aabbExtent(cb, 1);
            float ez = aabbExtent(cb, 2);
            if (ey > ex && ey >= ez) axis = 1;
            else if (ez > ex && ez >= ey) axis = 2;

            const std::uint32_t mid = first + count / 2;

            auto& idx = *triIndices;
            std::nth_element(idx.begin() + first, idx.begin() + mid, idx.begin() + first + count,
                [&](std::uint32_t a, std::uint32_t c)
                {
                    return triInfo[a].centroid[axis] < triInfo[c].centroid[axis];
                });

            const std::uint32_t left = buildNode(first, mid - first);
            const std::uint32_t right = buildNode(mid, first + count - mid);

            nodes[nodeIdx].meta[0] = left;
            nodes[nodeIdx].meta[1] = right;
            nodes[nodeIdx].meta[2] = 0;
            nodes[nodeIdx].meta[3] = 0;
            return nodeIdx;
        }
    };

    static constexpr int kBaseColorSamplerBinding = 0;
    static constexpr int kMaxBaseColorSamplers = 16;

    struct State
    {
        bool inited = false;
        bool paused = false;
        bool stepOnce = false;

        pt::Settings settings{};
        pt::Stats stats{};

        int viewportW = 0;
        int viewportH = 0;
        int internalW = 0;
        int internalH = 0;

        int maxCombinedTextureUnits = 0;
        int maxBaseColorSamplers = kMaxBaseColorSamplers;
        std::vector<GLuint> baseColorSamplers;

        std::uint64_t sppAccum = 0;
        std::uint32_t frameIndex = 0;

        bool forceTestPattern = false;
        int debugGLFrames = 0;

        // Textures
        GLuint texSampleHDR = 0;       // RGBA16F
        GLuint texAccumHDR = 0;        // RGBA16F
        GLuint texCount = 0;           // R32UI

        GLuint texAlbedo = 0;          // RGBA16F
        GLuint texNormal = 0;          // RG16F (oct)
        GLuint texDepth = 0;           // R32F
        GLuint texRoughMetal = 0;      // RG16F

        GLuint texDenoiseA = 0;        // RGBA16F
        GLuint texDenoiseB = 0;        // RGBA16F
        GLuint texOutputLDR = 0;       // RGBA8 (gamma encoded)

        // Scene SSBOs
        GLuint ssboNodes = 0;          // NodeGPU[]
        GLuint ssboTriIndices = 0;     // uint[]
        GLuint ssboTris = 0;           // TriGPU[]
        GLuint ssboMats = 0;           // MaterialGPU[]
        bool hasScene = false;
        std::uint32_t sceneTriCount = 0;
        std::uint32_t sceneNodeCount = 0;
        std::uint32_t sceneMatCount = 0;

        // Camera override
        bool camOverride = false;
        float camPos[3]{};
        float camDir[3]{};
        float camRight[3]{};
        float camUp[3]{};
        float camTanHalfFovY = 1.0f;

        // Programs
        GLProgram progClear{};
        GLProgram progTrace{};
        GLProgram progAccumulate{};
        GLProgram progAtrous{};
        GLProgram progTonemap{};

        // Timers
        GpuPassTimer tTrace{};
        GpuPassTimer tAcc{};
        GpuPassTimer tDenoise{};
        GpuPassTimer tTonemap{};
    };

    static State g;

    static const char* kClearCS = R"GLSL(
#version 460
layout(local_size_x=8, local_size_y=8) in;

layout(rgba16f, binding=0) uniform image2D gAccum;
layout(rgba16f, binding=1) uniform image2D gSample;
layout(r32ui,  binding=2) uniform uimage2D gCount;
layout(rgba16f, binding=3) uniform image2D gDenoiseA;
layout(rgba16f, binding=4) uniform image2D gDenoiseB;

uniform ivec2 uRes;

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uRes.x || p.y >= uRes.y) return;

    imageStore(gAccum, p, vec4(0));
    imageStore(gSample, p, vec4(0));
    imageStore(gCount, p, uvec4(0,0,0,0));
    imageStore(gDenoiseA, p, vec4(0));
    imageStore(gDenoiseB, p, vec4(0));
}
)GLSL";
    static const char* kTraceCS = R"GLSL(
#version 460
layout(local_size_x=8, local_size_y=8) in;

layout(rgba16f, binding=0) uniform image2D gSample;
layout(rgba16f, binding=1) uniform image2D gAlbedo;
layout(rg16f,  binding=2) uniform image2D gNormalOct;
layout(r32f,   binding=3) uniform image2D gDepth;
layout(rg16f,  binding=4) uniform image2D gRoughMetal;

uniform ivec2 uRes;
uniform uint  uSampleBase;
uniform int   uSpp;

uniform vec3  uCamPos;
uniform vec3  uCamDir;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform float uTanHalfFovY;
uniform float uAspect;

uniform int   uUseMeshScene;

// GPU scene buffers
struct NodeGPU { vec4 bmin; vec4 bmax; uvec4 meta; };
layout(std430, binding=10) readonly buffer Nodes     { NodeGPU nodes[]; };
layout(std430, binding=11) readonly buffer TriIdx    { uint triIdx[]; };

struct TriGPU { vec4 v0; vec4 e1; vec4 e2; vec4 n0; vec4 n1; vec4 n2; vec4 uv01; vec4 uv2; };
layout(std430, binding=12) readonly buffer Tris      { TriGPU tris[]; };

struct MaterialGPU { vec4 baseColor; vec4 emissiveRough; vec4 metallicPad; ivec4 tex; };
layout(std430, binding=13) readonly buffer Mats      { MaterialGPU mats[]; };

#define PT_MAX_BASECOLOR_TEX 16

layout(binding = 0) uniform sampler2D uBaseColorTex[PT_MAX_BASECOLOR_TEX];
uniform int uBaseColorTexCount;

uint hash_u32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint hash3(uvec3 v)
{
    return hash_u32(v.x ^ hash_u32(v.y ^ hash_u32(v.z)));
}

float rnd(inout uint state)
{
    state = 1664525U * state + 1013904223U;
    uint x = (state >> 8) | 0x3f800000U;
    return uintBitsToFloat(x) - 1.0;
}

vec2 octEncode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z) + 1e-8);
    vec2 p = n.xy;
    if (n.z < 0.0) p = (1.0 - abs(p.yx)) * sign(p.xy);
    return p;
}

bool intersectAABB(vec3 ro, vec3 invDir, vec3 bmin, vec3 bmax, out float tminOut, out float tmaxOut)
{
    vec3 t0 = (bmin - ro) * invDir;
    vec3 t1 = (bmax - ro) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tminV = max(max(tmin.x, tmin.y), tmin.z);
    float tmaxV = min(min(tmax.x, tmax.y), tmax.z);
    tminOut = tminV;
    tmaxOut = tmaxV;
    return tmaxV >= max(tminV, 0.0);
}

bool intersectTri(vec3 ro, vec3 rd, TriGPU tri, out float t, out float u, out float v)
{
    vec3 v0 = tri.v0.xyz;
    vec3 e1 = tri.e1.xyz;
    vec3 e2 = tri.e2.xyz;

    vec3 pvec = cross(rd, e2);
    float det = dot(e1, pvec);
    if (abs(det) < 1e-8) return false;

    float invDet = 1.0 / det;

    vec3 tvec = ro - v0;
    u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    vec3 qvec = cross(tvec, e1);
    v = dot(rd, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    t = dot(e2, qvec) * invDet;
    return t > 0.001;
}

bool traceMesh(vec3 ro, vec3 rd, out float tBest, out uint triBest, out float bu, out float bv)
{
    tBest = 1e30;
    triBest = 0u;
    bu = 0.0;
    bv = 0.0;

    vec3 invDir = 1.0 / rd;

    uint stack[64];
    int sp = 0;
    stack[sp++] = 0u;

    while (sp > 0)
    {
        uint ni = stack[--sp];
        NodeGPU n = nodes[ni];

        float tmin, tmax;
        if (!intersectAABB(ro, invDir, n.bmin.xyz, n.bmax.xyz, tmin, tmax)) continue;
        if (tmin > tBest) continue;

        if (n.meta.w > 0u)
        {
            uint first = n.meta.z;
            uint count = n.meta.w;
            for (uint i = 0u; i < count; ++i)
            {
                uint tid = triIdx[first + i];
                TriGPU tri = tris[tid];
                float t,u,v;
                if (intersectTri(ro, rd, tri, t, u, v) && t < tBest)
                {
                    tBest = t;
                    triBest = tid;
                    bu = u;
                    bv = v;
                }
            }
        }
        else
        {
            uint left = n.meta.x;
            uint right = n.meta.y;
            if (sp < 62) { stack[sp++] = left; stack[sp++] = right; }
        }
    }

    return tBest < 1e20;
}

bool occludedMesh(vec3 ro, vec3 rd, float tMax)
{
    vec3 invDir = 1.0 / rd;

    uint stack[64];
    int sp = 0;
    stack[sp++] = 0u;

    while (sp > 0)
    {
        uint ni = stack[--sp];
        NodeGPU n = nodes[ni];

        float tmin, tmax;
        if (!intersectAABB(ro, invDir, n.bmin.xyz, n.bmax.xyz, tmin, tmax)) continue;
        if (tmin > tMax) continue;

        if (n.meta.w > 0u)
        {
            uint first = n.meta.z;
            uint count = n.meta.w;
            for (uint i = 0u; i < count; ++i)
            {
                uint tid = triIdx[first + i];
                TriGPU tri = tris[tid];
                float t,u,v;
                if (intersectTri(ro, rd, tri, t, u, v) && t < tMax)
                    return true;
            }
        }
        else
        {
            uint left = n.meta.x;
            uint right = n.meta.y;
            if (sp < 62) { stack[sp++] = left; stack[sp++] = right; }
        }
    }

    return false;
}

vec3 environmentSky(vec3 rd)
{
    float t = 0.5 * (rd.y + 1.0);
    return mix(vec3(0.70, 0.80, 1.00), vec3(0.08, 0.08, 0.10), 1.0 - t);
}

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uRes.x || p.y >= uRes.y) return;

    vec3 sumL = vec3(0);
    vec3 outAlb = vec3(0);
    vec3 outNvs = vec3(0);
    float outDepth = 0.0;
    vec2 outRM = vec2(0.7, 0.0);

    for (int s = 0; s < max(uSpp,1); ++s)
    {
        uint rng = hash3(uvec3(uint(p.x), uint(p.y), uSampleBase + uint(s)));

        float jx = rnd(rng);
        float jy = rnd(rng);

        float fx = ((float(p.x) + jx) / float(uRes.x)) * 2.0 - 1.0;
        float fy = ((float(p.y) + jy) / float(uRes.y)) * 2.0 - 1.0;

        float px = fx * uAspect * uTanHalfFovY;
        float py = -fy * uTanHalfFovY;

        vec3 rd = normalize(uCamDir + px * uCamRight + py * uCamUp);
        vec3 ro = uCamPos;

        // Always start with environment so no path yields “all black”
        vec3 Lo = environmentSky(rd);

        if (uUseMeshScene != 0)
        {
            float tBest;
            uint triId;
            float bu, bv;

            if (traceMesh(ro, rd, tBest, triId, bu, bv))
            {
                TriGPU tri = tris[triId];
                float bw = 1.0 - bu - bv;

                vec3 hp = ro + rd * tBest;

                vec3 nInterp = tri.n0.xyz * bw + tri.n1.xyz * bu + tri.n2.xyz * bv;
                float n2 = dot(nInterp, nInterp);
                vec3 Ng = cross(tri.e1.xyz, tri.e2.xyz);
                float g2 = dot(Ng, Ng);
                vec3 N = (n2 > 1e-12) ? (nInterp * inversesqrt(n2)) :
                         ((g2 > 1e-12) ? (Ng * inversesqrt(g2)) : vec3(0.0, 1.0, 0.0));

                uint matId = floatBitsToUint(tri.v0.w);
                MaterialGPU m = mats[matId];

                vec2 uv0 = tri.uv01.xy;
                vec2 uv1 = tri.uv01.zw;
                vec2 uv2 = tri.uv2.xy;
                vec2 uv  = uv0 * bw + uv1 * bu + uv2 * bv;

                vec3 alb = m.baseColor.rgb;
                if (m.tex.x >= 0 && m.tex.x < uBaseColorTexCount)
                {
                    alb *= texture(uBaseColorTex[m.tex.x], uv).rgb;
                }

                vec3 emissive = m.emissiveRough.rgb;
                float rough = m.emissiveRough.a;
                float metal = m.metallicPad.x;

                // Headlight + sky (very hard to end up black)
                vec3 Ldir = normalize(-rd);
                float ndotl = max(dot(N, Ldir), 0.0);
                vec3 diffuse = alb * (ndotl / 3.14159265) * vec3(2.0);

                // Optional shadow ray toward a fixed point light (adds depth)
                vec3 lightPos = vec3(0.0, 4.0, 1.5);
                vec3 toL = lightPos - hp;
                float dist2 = dot(toL, toL);
                float dist = sqrt(max(dist2, 1e-8));
                vec3 ldir = toL / dist;
                float nl = max(dot(N, ldir), 0.0);
                bool occ = false;
                if (nl > 0.0)
                    occ = occludedMesh(hp + N * 0.001, ldir, dist - 0.002);

                vec3 Li = (!occ) ? (vec3(1.0, 0.98, 0.92) * (65.0 / dist2)) : vec3(0);
                vec3 direct = alb * (nl / 3.14159265) * Li;

                Lo = emissive + diffuse + direct;

                outAlb += alb;
                outNvs += N;
                outDepth += dot((hp - uCamPos), uCamDir);
                outRM = vec2(rough, metal);
            }
            else
            {
                // Stable AOVs on miss
                outAlb += vec3(0.0);
                outNvs += vec3(0.0, 1.0, 0.0);
                outDepth += 1.0e10;
            }
        }
        else
        {
            // If no mesh scene, still show sky
            outAlb += vec3(0.0);
            outNvs += vec3(0.0, 1.0, 0.0);
            outDepth += 1.0e10;
        }

        sumL += Lo;
    }

    float inv = 1.0 / float(max(uSpp,1));
    vec3 L = sumL * inv;
    vec3 A = outAlb * inv;

    vec3 nTmp = outNvs * inv;
    float nn = dot(nTmp, nTmp);
    vec3 N = (nn > 1e-12) ? (nTmp * inversesqrt(nn)) : vec3(0.0, 1.0, 0.0);

    float D = outDepth * inv;

    imageStore(gSample, p, vec4(L, 1.0));
    imageStore(gAlbedo, p, vec4(A, 1.0));

    vec2 oct = octEncode(N);
    imageStore(gNormalOct, p, vec4(oct, 0.0, 0.0));
    imageStore(gDepth, p, vec4(D,0,0,0));
    imageStore(gRoughMetal, p, vec4(outRM, 0.0, 0.0));
}
)GLSL";

    static const char* kAccumulateCS = R"GLSL(
#version 460
layout(local_size_x=8, local_size_y=8) in;

layout(rgba16f, binding=0) uniform image2D gSample;
layout(rgba16f, binding=1) uniform image2D gAccum;
layout(r32ui,  binding=2) uniform uimage2D gCount;

uniform ivec2 uRes;

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uRes.x || p.y >= uRes.y) return;

    vec4 s = imageLoad(gSample, p);
    uvec4 c = imageLoad(gCount, p);
    uint n = c.x + 1u;

    vec4 a = imageLoad(gAccum, p);
    vec4 outv = (a * float(c.x) + s) / float(n);

    imageStore(gAccum, p, outv);
    imageStore(gCount, p, uvec4(n,0,0,0));
}
)GLSL";

    static const char* kAtrousCS = R"GLSL(
#version 460
layout(local_size_x=8, local_size_y=8) in;

layout(rgba16f, binding=0) uniform readonly image2D gIn;
layout(rgba16f, binding=1) uniform writeonly image2D gOut;
layout(rg16f,  binding=2) uniform readonly image2D gNormalOct;
layout(r32f,   binding=3) uniform readonly image2D gDepth;

uniform ivec2 uRes;
uniform int   uStep;
uniform float uSigmaZ;
uniform float uSigmaN;

vec3 octDecode(vec2 e)
{
    vec3 v = vec3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

float wNormal(vec3 n0, vec3 n1) { return pow(max(dot(n0,n1), 0.0), uSigmaN); }
float wDepth(float z0, float z1) { float dz = abs(z0 - z1); return exp(-dz * uSigmaZ); }

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uRes.x || p.y >= uRes.y) return;

    vec4 c0 = imageLoad(gIn, p);
    vec2 o0 = imageLoad(gNormalOct, p).xy;
    float z0 = imageLoad(gDepth, p).x;

    vec3 n0 = octDecode(o0);

    const int step = uStep;
    const int k[5] = int[5](-2,-1,0,1,2);
    const float w[5] = float[5](0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

    vec4 sum = vec4(0);
    float wsum = 0.0;

    for (int yy=0; yy<5; ++yy)
    for (int xx=0; xx<5; ++xx)
    {
        ivec2 q = p + ivec2(k[xx]*step, k[yy]*step);
        if (q.x < 0 || q.y < 0 || q.x >= uRes.x || q.y >= uRes.y) continue;

        vec4 ci = imageLoad(gIn, q);
        vec2 oi = imageLoad(gNormalOct, q).xy;
        float zi = imageLoad(gDepth, q).x;

        vec3 ni = octDecode(oi);

        float ww = w[xx] * w[yy];
        ww *= wNormal(n0, ni);
        ww *= wDepth(z0, zi);

        sum += ci * ww;
        wsum += ww;
    }

    vec4 outv = (wsum > 0.0) ? (sum / wsum) : c0;
    imageStore(gOut, p, outv);
}
)GLSL";
    static const char* kTonemapCS = R"GLSL(
#version 460
layout(local_size_x=8, local_size_y=8) in;

layout(rgba16f, binding=0) uniform readonly image2D gHDR;
layout(rgba16f, binding=1) uniform readonly image2D gAlbedo;
layout(rg16f,  binding=2) uniform readonly image2D gNormalOct;
layout(r32f,   binding=3) uniform readonly image2D gDepth;
layout(rg16f,  binding=4) uniform readonly image2D gRoughMetal;

layout(rgba8,  binding=5) uniform writeonly image2D gOutLDR;

uniform ivec2 uOutRes;
uniform ivec2 uInRes;
uniform int   uViewMode;     // pt::DebugView
uniform float uExposureEV;
uniform int   uForceTestPattern;

vec3 octDecode(vec2 e)
{
    vec3 v = vec3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

vec3 tonemapACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

vec3 toSRGB(vec3 c)
{
    return pow(max(c, vec3(0.0)), vec3(1.0/2.2));
}

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uOutRes.x || p.y >= uOutRes.y) return;

    if (uForceTestPattern != 0)
    {
        vec2 uv = vec2(p) / vec2(max(uOutRes, ivec2(1)));
        vec3 col = vec3(uv.x, uv.y, 1.0);
        imageStore(gOutLDR, p, vec4(col, 1.0));
        return;
    }

    ivec2 q = ivec2( int(float(p.x) * float(uInRes.x) / float(uOutRes.x)),
                     int(float(p.y) * float(uInRes.y) / float(uOutRes.y)) );
    q = clamp(q, ivec2(0), uInRes - ivec2(1));

    vec3 outc = imageLoad(gHDR, q).rgb;

    // Views (match pt::DebugView)
    if (uViewMode == 3) // Albedo
        outc = imageLoad(gAlbedo, q).rgb;
    else if (uViewMode == 4) // Normal
    {
        vec3 n = octDecode(imageLoad(gNormalOct, q).xy);
        outc = n * 0.5 + 0.5;
    }
    else if (uViewMode == 5) // Depth
    {
        float d = imageLoad(gDepth, q).x;
        outc = vec3(d * 0.02);
    }
    else if (uViewMode == 6) // Rough/Metal
    {
        vec2 rm = imageLoad(gRoughMetal, q).xy;
        outc = vec3(rm.x, rm.y, 0.0);
    }

    if (any(isnan(outc)) || any(isinf(outc))) outc = vec3(0.0);

    float exposure = exp2(uExposureEV);
    outc *= exposure;

    if (any(isnan(outc)) || any(isinf(outc))) outc = vec3(0.0);

    outc = tonemapACES(outc);
    outc = toSRGB(outc);

    imageStore(gOutLDR, p, vec4(outc, 1.0)); // alpha forced to 1
}
)GLSL";

    static void Dispatch2D(int w, int h)
    {
        if (w < 1 || h < 1) return;

        const GLuint gx = (GLuint)((w + 7) / 8);
        const GLuint gy = (GLuint)((h + 7) / 8);
        if (gx == 0 || gy == 0) return;

        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    static void DestroyAllTextures()
    {
        DestroyTex(g.texSampleHDR);
        DestroyTex(g.texAccumHDR);
        DestroyTex(g.texCount);

        DestroyTex(g.texAlbedo);
        DestroyTex(g.texNormal);
        DestroyTex(g.texDepth);
        DestroyTex(g.texRoughMetal);

        DestroyTex(g.texDenoiseA);
        DestroyTex(g.texDenoiseB);

        DestroyTex(g.texOutputLDR);

        g.viewportW = g.viewportH = 0;
        g.internalW = g.internalH = 0;
    }

    static bool EnsureResources(int viewportW, int viewportH)
    {
        const int newVW = std::max(1, viewportW);
        const int newVH = std::max(1, viewportH);

        const int prevVW = g.viewportW;
        const int prevVH = g.viewportH;

        g.viewportW = newVW;
        g.viewportH = newVH;

        const float rs = std::clamp(g.settings.renderScale, 0.05f, 1.0f);
        const int iw = std::max(1, (int)std::floor(float(newVW) * rs));
        const int ih = std::max(1, (int)std::floor(float(newVH) * rs));

        const bool internalMissing =
            !IsTexLevelDefined2D(g.texSampleHDR) || !IsTexLevelDefined2D(g.texAccumHDR) || !IsTexLevelDefined2D(g.texCount) ||
            !IsTexLevelDefined2D(g.texAlbedo) || !IsTexLevelDefined2D(g.texNormal) || !IsTexLevelDefined2D(g.texDepth) || !IsTexLevelDefined2D(g.texRoughMetal) ||
            !IsTexLevelDefined2D(g.texDenoiseA) || !IsTexLevelDefined2D(g.texDenoiseB);

        const bool outputMissing = !IsTexLevelDefined2D(g.texOutputLDR);

        const bool needInternalRecreate = (iw != g.internalW) || (ih != g.internalH) || internalMissing;
        const bool needOutputRecreate = (newVW != prevVW) || (newVH != prevVH) || outputMissing;

        if (!needInternalRecreate && !needOutputRecreate)
            return true;

        if (needInternalRecreate)
        {
            DestroyAllTextures();

            g.viewportW = newVW;
            g.viewportH = newVH;
            g.internalW = iw;
            g.internalH = ih;

            g.texSampleHDR = CreateTex2D(iw, ih, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, "PT_SampleHDR");
            g.texAccumHDR = CreateTex2D(iw, ih, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, "PT_AccumHDR");
            g.texCount = CreateTex2D(iw, ih, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false, "PT_Count");

            g.texAlbedo = CreateTex2D(iw, ih, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, "PT_Albedo");
            g.texNormal = CreateTex2D(iw, ih, GL_RG16F, GL_RG, GL_HALF_FLOAT, false, "PT_NormalOct");
            g.texDepth = CreateTex2D(iw, ih, GL_R32F, GL_RED, GL_FLOAT, false, "PT_Depth");
            g.texRoughMetal = CreateTex2D(iw, ih, GL_RG16F, GL_RG, GL_HALF_FLOAT, false, "PT_RoughMetal");

            g.texDenoiseA = CreateTex2D(iw, ih, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, "PT_DenoiseA");
            g.texDenoiseB = CreateTex2D(iw, ih, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, "PT_DenoiseB");

            g.texOutputLDR = CreateTex2D(newVW, newVH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true, "PT_OutputLDR");

            if (!IsTexLevelDefined2D(g.texSampleHDR) || !IsTexLevelDefined2D(g.texAccumHDR) || !IsTexLevelDefined2D(g.texCount) ||
                !IsTexLevelDefined2D(g.texAlbedo) || !IsTexLevelDefined2D(g.texNormal) || !IsTexLevelDefined2D(g.texDepth) || !IsTexLevelDefined2D(g.texRoughMetal) ||
                !IsTexLevelDefined2D(g.texDenoiseA) || !IsTexLevelDefined2D(g.texDenoiseB) || !IsTexLevelDefined2D(g.texOutputLDR))
            {
                std::fprintf(stderr, "[PT][Res] Resource allocation failed; destroying PT textures to avoid GL error cascade.\n");
                DestroyAllTextures();
                return false;
            }

            g.settings.resetAccumulation = true;
            g.debugGLFrames = 4;
            return true;
        }

        if (needOutputRecreate)
        {
            DestroyTex(g.texOutputLDR);
            g.texOutputLDR = CreateTex2D(newVW, newVH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true, "PT_OutputLDR");

            if (!IsTexLevelDefined2D(g.texOutputLDR))
            {
                std::fprintf(stderr, "[PT][Res] Failed to allocate PT_OutputLDR %dx%d; skipping PT this frame.\n", newVW, newVH);
                return false;
            }

            g.debugGLFrames = 4;
        }

        return true;
    }

    static void BindSceneSSBOs()
    {
        if (!g.hasScene) return;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, g.ssboNodes);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, g.ssboTriIndices);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, g.ssboTris);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, g.ssboMats);
    }

    static void RunClear()
    {
        glUseProgram(g.progClear.id);

        glUniform2i(glGetUniformLocation(g.progClear.id, "uRes"), g.internalW, g.internalH);

        glBindImageTexture(0, g.texAccumHDR, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunAccumulate(bind)");
        glBindImageTexture(1, g.texSampleHDR, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunSample(bind)");
        glBindImageTexture(2, g.texCount, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        if (g.debugGLFrames > 0) DrainGLErrors("RunCount(bind)");
        glBindImageTexture(3, g.texDenoiseA, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunDenoiseA(bind)");
        glBindImageTexture(4, g.texDenoiseB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunDenoiseB(bind)");

        Dispatch2D(g.internalW, g.internalH);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(0);
    }

    static void RunTrace()
    {
        glUseProgram(g.progTrace.id);

        glUniform2i(glGetUniformLocation(g.progTrace.id, "uRes"), g.internalW, g.internalH);
        glUniform1ui(glGetUniformLocation(g.progTrace.id, "uSampleBase"), (GLuint)g.sppAccum);
        glUniform1i(glGetUniformLocation(g.progTrace.id, "uSpp"), g.settings.sppPerFrame);

        const float aspect = float(g.internalW) / float(g.internalH);

        if (g.camOverride)
        {
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamPos"), g.camPos[0], g.camPos[1], g.camPos[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamDir"), g.camDir[0], g.camDir[1], g.camDir[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamRight"), g.camRight[0], g.camRight[1], g.camRight[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamUp"), g.camUp[0], g.camUp[1], g.camUp[2]);
            glUniform1f(glGetUniformLocation(g.progTrace.id, "uTanHalfFovY"), g.camTanHalfFovY);
            glUniform1f(glGetUniformLocation(g.progTrace.id, "uAspect"), aspect);
        }
        else
        {
            // fallback camera
            const float fovY = 45.0f * 3.14159265f / 180.0f;
            const float tanHalf = std::tan(0.5f * fovY);
            const float camPos[3] = { 2.8f, 1.6f, 3.2f };
            const float camDir[3] = { -0.65f, -0.25f, -0.72f };
            const float camRight[3] = { 0.74f, 0.0f, -0.67f };
            const float camUp[3] = { -0.17f, 0.97f, -0.19f };

            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamPos"), camPos[0], camPos[1], camPos[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamDir"), camDir[0], camDir[1], camDir[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamRight"), camRight[0], camRight[1], camRight[2]);
            glUniform3f(glGetUniformLocation(g.progTrace.id, "uCamUp"), camUp[0], camUp[1], camUp[2]);
            glUniform1f(glGetUniformLocation(g.progTrace.id, "uTanHalfFovY"), tanHalf);
            glUniform1f(glGetUniformLocation(g.progTrace.id, "uAspect"), aspect);
        }

        glUniform1i(glGetUniformLocation(g.progTrace.id, "uUseMeshScene"), g.hasScene ? 1 : 0);
        if (g.hasScene) BindSceneSSBOs();

        glBindImageTexture(0, g.texSampleHDR, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunSample(bind)");
        glBindImageTexture(1, g.texAlbedo, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunAlbedo(bind)");
        glBindImageTexture(2, g.texNormal, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunNormal(bind)");
        glBindImageTexture(3, g.texDepth, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunDepth(bind)");
        glBindImageTexture(4, g.texRoughMetal, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunMetal(bind)");

        const int bcCount = std::min((int)g.baseColorSamplers.size(), g.maxBaseColorSamplers);

        if (GLint loc = glGetUniformLocation(g.progTrace.id, "uBaseColorTexCount"); loc >= 0)
            glUniform1i(loc, bcCount);

        for (int i = 0; i < bcCount; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + kBaseColorSamplerBinding + i);
            glBindTexture(GL_TEXTURE_2D, g.baseColorSamplers[i]);
        }
        glActiveTexture(GL_TEXTURE0);

        Dispatch2D(g.internalW, g.internalH);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(0);
    }

    static void RunAccumulate()
    {
        glUseProgram(g.progAccumulate.id);

        glUniform2i(glGetUniformLocation(g.progAccumulate.id, "uRes"), g.internalW, g.internalH);

        glBindImageTexture(0, g.texSampleHDR, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunSample(bind)");
        glBindImageTexture(1, g.texAccumHDR, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunAccumulate(bind)");
        glBindImageTexture(2, g.texCount, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        if (g.debugGLFrames > 0) DrainGLErrors("RunCount(bind)");

        Dispatch2D(g.internalW, g.internalH);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(0);
    }

    static GLuint RunAtrous(GLuint inputTex)
    {
        GLuint inTex = inputTex;
        GLuint outTex = g.texDenoiseA;

        for (int i = 0; i < 3; ++i)
        {
            const int step = 1 << i;

            glUseProgram(g.progAtrous.id);
            glUniform2i(glGetUniformLocation(g.progAtrous.id, "uRes"), g.internalW, g.internalH);
            glUniform1i(glGetUniformLocation(g.progAtrous.id, "uStep"), step);
            glUniform1f(glGetUniformLocation(g.progAtrous.id, "uSigmaZ"), 2.0f);
            glUniform1f(glGetUniformLocation(g.progAtrous.id, "uSigmaN"), 48.0f);

            glBindImageTexture(0, inTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
            if (g.debugGLFrames > 0) DrainGLErrors("RunInTex(bind)");
            glBindImageTexture(1, outTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            if (g.debugGLFrames > 0) DrainGLErrors("RunOutTex(bind)");
            glBindImageTexture(2, g.texNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);
            if (g.debugGLFrames > 0) DrainGLErrors("RunNormal(bind)");
            glBindImageTexture(3, g.texDepth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
            if (g.debugGLFrames > 0) DrainGLErrors("RunDepth(bind)");

            Dispatch2D(g.internalW, g.internalH);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(0);

            inTex = outTex;
            outTex = (outTex == g.texDenoiseA) ? g.texDenoiseB : g.texDenoiseA;
        }

        return inTex;
    }

    static void RunTonemap(GLuint hdrSource, int viewMode)
    {
        glUseProgram(g.progTonemap.id);

        glUniform2i(glGetUniformLocation(g.progTonemap.id, "uOutRes"), g.viewportW, g.viewportH);
        glUniform2i(glGetUniformLocation(g.progTonemap.id, "uInRes"), g.internalW, g.internalH);
        glUniform1i(glGetUniformLocation(g.progTonemap.id, "uViewMode"), viewMode);
        glUniform1f(glGetUniformLocation(g.progTonemap.id, "uExposureEV"), g.settings.exposureEV);
        glUniform1i(glGetUniformLocation(g.progTonemap.id, "uForceTestPattern"), g.forceTestPattern ? 1 : 0);

        glBindImageTexture(0, hdrSource, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunSource(bind)");
        glBindImageTexture(1, g.texAlbedo, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunAlbedo(bind)");
        glBindImageTexture(2, g.texNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunNormal(bind)");
        glBindImageTexture(3, g.texDepth, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunDepth(bind)");
        glBindImageTexture(4, g.texRoughMetal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);
        if (g.debugGLFrames > 0) DrainGLErrors("RunMetal(bind)");
        glBindImageTexture(5, g.texOutputLDR, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        if (g.debugGLFrames > 0) DrainGLErrors("RunTonemap(bind)");


        Dispatch2D(g.viewportW, g.viewportH);

        // Allow subsequent sampling of g.texOutputLDR in the same frame
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        glUseProgram(0);
    }

    static void UploadSceneInternal(const pt::TriInput* tris, std::size_t triCount,
        const pt::MaterialInput* mats, std::size_t matCount)
    {
        std::fprintf(stderr, "[PT] UploadScene tris=%zu mats=%zu\n", triCount, matCount);
        if (!g.inited) return;

        if (!tris || triCount == 0 || !mats || matCount == 0)
        {
            g.hasScene = false;
            g.sceneTriCount = 0;
            g.sceneNodeCount = 0;
            g.sceneMatCount = 0;
            return;
        }

        g.baseColorSamplers.clear();

        std::vector<MaterialGPU> mg(matCount);
        for (std::size_t i = 0; i < matCount; ++i)
        {
            MaterialGPU m{};
            m.baseColor[0] = mats[i].baseColor[0];
            m.baseColor[1] = mats[i].baseColor[1];
            m.baseColor[2] = mats[i].baseColor[2];
            m.baseColor[3] = mats[i].baseColor[3];

            m.emissiveRough[0] = mats[i].emissive[0];
            m.emissiveRough[1] = mats[i].emissive[1];
            m.emissiveRough[2] = mats[i].emissive[2];
            m.emissiveRough[3] = mats[i].roughness;

            m.metallicPad[0] = mats[i].metallic;
            m.metallicPad[1] = 0.0f;
            m.metallicPad[2] = 0.0f;
            m.metallicPad[3] = 0.0f;

            // Default: no textures.
            m.tex[0] = -1; m.tex[1] = -1; m.tex[2] = -1; m.tex[3] = -1;

            // BaseColor texture mapping (GL texture id -> sampler index)
            if (mats[i].baseColorTexGL != 0 && g.maxBaseColorSamplers > 0)
            {
                const GLuint texId = (GLuint)mats[i].baseColorTexGL;

                int found = -1;
                for (int t = 0; t < (int)g.baseColorSamplers.size(); ++t)
                {
                    if (g.baseColorSamplers[t] == texId) { found = t; break; }
                }

                if (found < 0)
                {
                    if ((int)g.baseColorSamplers.size() < g.maxBaseColorSamplers)
                    {
                        found = (int)g.baseColorSamplers.size();
                        g.baseColorSamplers.push_back(texId);
                    }
                }

                m.tex[0] = found;
            }

            mg[i] = m;
        }

        auto packU32ToF = [](std::uint32_t u) -> float
            {
                union { std::uint32_t u; float f; } v{ u };
                return v.f;
            };

        std::vector<TriGPU> tg(triCount);
        std::vector<TriBounds> info(triCount);

        for (std::size_t i = 0; i < triCount; ++i)
        {
            const pt::TriInput& t = tris[i];

            float v0[3]{ t.v0[0], t.v0[1], t.v0[2] };
            float v1[3]{ t.v1[0], t.v1[1], t.v1[2] };
            float v2[3]{ t.v2[0], t.v2[1], t.v2[2] };

            TriGPU out{};
            out.v0[0] = v0[0]; out.v0[1] = v0[1]; out.v0[2] = v0[2]; out.v0[3] = packU32ToF(t.material);
            out.e1[0] = v1[0] - v0[0]; out.e1[1] = v1[1] - v0[1]; out.e1[2] = v1[2] - v0[2]; out.e1[3] = 0.0f;
            out.e2[0] = v2[0] - v0[0]; out.e2[1] = v2[1] - v0[1]; out.e2[2] = v2[2] - v0[2]; out.e2[3] = 0.0f;

            out.n0[0] = t.n0[0]; out.n0[1] = t.n0[1]; out.n0[2] = t.n0[2]; out.n0[3] = 0.0f;
            out.n1[0] = t.n1[0]; out.n1[1] = t.n1[1]; out.n1[2] = t.n1[2]; out.n1[3] = 0.0f;
            out.n2[0] = t.n2[0]; out.n2[1] = t.n2[1]; out.n2[2] = t.n2[2]; out.n2[3] = 0.0f;

            // UVs (barycentric interpolation in shader)
            out.uv01[0] = t.uv0[0]; out.uv01[1] = t.uv0[1];
            out.uv01[2] = t.uv1[0]; out.uv01[3] = t.uv1[1];

            out.uv2[0] = t.uv2[0]; out.uv2[1] = t.uv2[1];
            out.uv2[2] = 0.0f;     out.uv2[3] = 0.0f;

            tg[i] = out;

            TriBounds tb{};
            tb.bounds = aabbEmpty();
            aabbGrow(tb.bounds, v0);
            aabbGrow(tb.bounds, v1);
            aabbGrow(tb.bounds, v2);
            tb.centroid[0] = (v0[0] + v1[0] + v2[0]) / 3.0f;
            tb.centroid[1] = (v0[1] + v1[1] + v2[1]) / 3.0f;
            tb.centroid[2] = (v0[2] + v1[2] + v2[2]) / 3.0f;
            info[i] = tb;
        }

        std::vector<std::uint32_t> triIndices(triCount);
        for (std::uint32_t i = 0; i < (std::uint32_t)triCount; ++i) triIndices[i] = i;

        BuildContext bc{};
        bc.triIndices = &triIndices;
        bc.triInfo = std::move(info);
        bc.nodes.reserve(std::max<std::size_t>(1, triCount * 2));
        (void)bc.buildNode(0, (std::uint32_t)triCount);

        if (!g.ssboNodes) glGenBuffers(1, &g.ssboNodes);
        if (!g.ssboTriIndices) glGenBuffers(1, &g.ssboTriIndices);
        if (!g.ssboTris) glGenBuffers(1, &g.ssboTris);
        if (!g.ssboMats) glGenBuffers(1, &g.ssboMats);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.ssboNodes);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bc.nodes.size() * sizeof(NodeGPU), bc.nodes.data(), GL_STATIC_COPY);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.ssboTriIndices);
        glBufferData(GL_SHADER_STORAGE_BUFFER, triIndices.size() * sizeof(std::uint32_t), triIndices.data(), GL_STATIC_COPY);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.ssboTris);
        glBufferData(GL_SHADER_STORAGE_BUFFER, tg.size() * sizeof(TriGPU), tg.data(), GL_STATIC_COPY);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.ssboMats);
        glBufferData(GL_SHADER_STORAGE_BUFFER, mg.size() * sizeof(MaterialGPU), mg.data(), GL_STATIC_COPY);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        g.hasScene = (triCount > 0 && bc.nodes.size() > 0 && matCount > 0);
        g.sceneTriCount = (std::uint32_t)triCount;
        g.sceneNodeCount = (std::uint32_t)bc.nodes.size();
        g.sceneMatCount = (std::uint32_t)matCount;

        g.settings.resetAccumulation = true;
        printf("[PT] baseColorSamplers=%d\n", (int)g.baseColorSamplers.size());
    }
}

namespace pt
{
    Settings& GetSettings() { return g.settings; }
    const Stats& GetStats() { return g.stats; }
    std::uint32_t GetOutputTextureGL() { return (std::uint32_t)g.texOutputLDR; }

    bool Initialize()
    {
        if (g.inited) return true;

        if (!GLAD_GL_VERSION_4_3)
        {
            std::fprintf(stderr, "[PathTracerGL] Requires OpenGL 4.3+.\n");
            return false;
        }

        int maxComputeTexUnits = 0;
        glGetIntegerv(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &maxComputeTexUnits);

        // Shader cap is PT_MAX_BASECOLOR_TEX (=16). Binding base is kBaseColorSamplerBinding (=0).
        g.maxBaseColorSamplers = std::max(0, std::min(kMaxBaseColorSamplers, maxComputeTexUnits - kBaseColorSamplerBinding));

        printf("[PT] GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS=%d, baseColor sampler slots=%d (binding base=%d)\n",
            maxComputeTexUnits, g.maxBaseColorSamplers, kBaseColorSamplerBinding);

        g.progClear = MakeComputeProgram(kClearCS, "PT_Clear");
        g.progTrace = MakeComputeProgram(kTraceCS, "PT_Trace");
        g.progAccumulate = MakeComputeProgram(kAccumulateCS, "PT_Accumulate");
        g.progAtrous = MakeComputeProgram(kAtrousCS, "PT_Atrous");
        g.progTonemap = MakeComputeProgram(kTonemapCS, "PT_Tonemap");

        g.tTrace.init();
        g.tAcc.init();
        g.tDenoise.init();
        g.tTonemap.init();

        g.inited = true;
        return true;
    }

    void Shutdown()
    {
        if (!g.inited) return;

        ClearScene();
        DestroyAllTextures();

        g.progClear.destroy();
        g.progTrace.destroy();
        g.progAccumulate.destroy();
        g.progAtrous.destroy();
        g.progTonemap.destroy();

        g.tTrace.shutdown();
        g.tAcc.shutdown();
        g.tDenoise.shutdown();
        g.tTonemap.shutdown();

        g.inited = false;
    }

    void RequestReset()
    {
        g.settings.resetAccumulation = true;
    }

    void pt::SetCameraBasis(const float pos[3], const float dir[3], const float right[3], const float up[3], float tanHalfFovY)
    {
        // Detect camera changes and reset accumulation
        bool changed = !g.camOverride;

        if (g.camOverride)
        {
            auto dist2 = [](const float a[3], const float b[3]) -> float
                {
                    const float dx = a[0] - b[0];
                    const float dy = a[1] - b[1];
                    const float dz = a[2] - b[2];
                    return dx * dx + dy * dy + dz * dz;
                };

            auto dot3 = [](const float a[3], const float b[3]) -> float
                {
                    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
                };

            // Position: reset if moved more than ~1e-4 units (squared threshold).
            changed |= (dist2(g.camPos, pos) > 1e-8f);

            // Orientation: reset if axes changed enough (1 - dot is a small-angle proxy).
            changed |= ((1.0f - dot3(g.camDir, dir)) > 1e-6f);
            changed |= ((1.0f - dot3(g.camRight, right)) > 1e-6f);
            changed |= ((1.0f - dot3(g.camUp, up)) > 1e-6f);

            // FOV
            changed |= (std::fabs(g.camTanHalfFovY - tanHalfFovY) > 1e-5f);
        }

        g.camOverride = true;
        g.camPos[0] = pos[0];   g.camPos[1] = pos[1];   g.camPos[2] = pos[2];
        g.camDir[0] = dir[0];   g.camDir[1] = dir[1];   g.camDir[2] = dir[2];
        g.camRight[0] = right[0]; g.camRight[1] = right[1]; g.camRight[2] = right[2];
        g.camUp[0] = up[0];     g.camUp[1] = up[1];     g.camUp[2] = up[2];
        g.camTanHalfFovY = tanHalfFovY;

        if (changed)
            pt::RequestReset();
    }


    void ClearScene()
    {
        DestroySSBO(g.ssboNodes);
        DestroySSBO(g.ssboTriIndices);
        DestroySSBO(g.ssboTris);
        DestroySSBO(g.ssboMats);

        g.hasScene = false;
        g.sceneTriCount = 0;
        g.sceneNodeCount = 0;
        g.sceneMatCount = 0;

        RequestReset();
    }

    bool HasScene() { return g.hasScene; }

    void UploadScene(const TriInput* tris, std::size_t triCount,
        const MaterialInput* mats, std::size_t matCount)
    {
        if (!g.inited) return;
        UploadSceneInternal(tris, triCount, mats, matCount);
        RequestReset();
    }

    void DrawImGuiPanel()
    {
        if (!g.inited) return;

        static int s_lastFrame = -1;
        const int f = ImGui::GetFrameCount();
        if (s_lastFrame == f) return;
        s_lastFrame = f;

        ImGui::PushID("PathTracerGL");

        if (ImGui::Begin("Path Tracer"))
        {
            bool enabled = g.settings.enabled;
            if (ImGui::Checkbox("Enabled", &enabled))
                g.settings.enabled = enabled;

            ImGui::SameLine();
            if (ImGui::Checkbox("Pause rendering", &g.paused))
            {
                if (!g.paused) RequestReset();
            }

            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
                g.paused = true;
                g.stepOnce = true;
            }

            int spp = g.settings.sppPerFrame;
            if (ImGui::SliderInt("SPP / frame", &spp, 1, 8))
            {
                g.settings.sppPerFrame = spp;
                RequestReset();
            }

            float rs = g.settings.renderScale;
            if (ImGui::SliderFloat("Render scale", &rs, 0.25f, 1.0f, "%.2f"))
            {
                g.settings.renderScale = rs;
                RequestReset();
            }

            float ev = g.settings.exposureEV;
            if (ImGui::SliderFloat("Exposure (EV)", &ev, -6.0f, 6.0f, "%.2f"))
                g.settings.exposureEV = ev;

            int den = (int)g.settings.denoiser;
            const char* denItems[] = { "None", "Atrous (GL)" };
            if (ImGui::Combo("Denoiser", &den, denItems, 2))
                g.settings.denoiser = (Denoiser)den;

            int view = (int)g.settings.view;
            const char* viewItems[] = { "Denoised", "Accumulated", "Sample", "Albedo", "Normal", "Depth", "Rough/Metal" };
            if (ImGui::Combo("View", &view, viewItems, 7))
                g.settings.view = (DebugView)view;

            ImGui::Checkbox("Force test pattern", &g.forceTestPattern); // no reset needed

            if (ImGui::Button("Reset accumulation"))
                RequestReset();

            ImGui::Separator();

            ImGui::Text("Internal: %dx%d", g.stats.internalW, g.stats.internalH);
            ImGui::Text("OutTex(GL): %u", (unsigned)g.texOutputLDR);
            ImGui::Text("SPP accumulated: %llu", (unsigned long long)g.stats.sppAccumulated);

            ImGui::Text("Mesh scene: %s", g.hasScene ? "YES" : "NO");
            if (g.hasScene)
            {
                ImGui::Text("Triangles: %u", g.sceneTriCount);
                ImGui::Text("BVH nodes:  %u", g.sceneNodeCount);
                ImGui::Text("Materials:  %u", g.sceneMatCount);
            }

            ImGui::Separator();
            ImGui::Text("Trace:      %.3f ms", g.stats.msPathTrace);
            ImGui::Text("Accumulate: %.3f ms", g.stats.msAccumulate);
            ImGui::Text("Denoise:    %.3f ms", g.stats.msDenoise);
            ImGui::Text("Tonemap:    %.3f ms", g.stats.msTonemap);
        }
        ImGui::End();

        ImGui::PopID();
    }

    void Render(int viewportW, int viewportH)
    {
        if (!g.inited) return;
        if (!g.settings.enabled) return;

        if (!EnsureResources(viewportW, viewportH))
            return;

        if (g.internalW < 1 || g.internalH < 1 || g.viewportW < 1 || g.viewportH < 1 || !IsTexLevelDefined2D(g.texOutputLDR))
            return;

        if (g.forceTestPattern)
        {
            const unsigned char magenta[4] = { 255, 0, 255, 255 }; // RGBA8

            glClearTexImage(g.texOutputLDR, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);

            if (g.debugGLFrames > 0) DrainGLErrors("TestPattern(clear)");
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

            if (g.debugGLFrames > 0) --g.debugGLFrames;
            return;
        }

        g.tTrace.resolve((int)g.frameIndex);
        g.tAcc.resolve((int)g.frameIndex);
        g.tDenoise.resolve((int)g.frameIndex);
        g.tTonemap.resolve((int)g.frameIndex);

        if (g.settings.resetAccumulation)
        {
            RunClear();
            g.sppAccum = 0;
            g.settings.resetAccumulation = false;
        }
        const bool doTraceWork = (!g.paused) || g.stepOnce;
        if (!doTraceWork)
        {
            return;
        }
        g.stepOnce = false;

        // Trace
        g.tTrace.begin((int)g.frameIndex);
        RunTrace();
        g.tTrace.end((int)g.frameIndex);

        // Accumulate
        g.tAcc.begin((int)g.frameIndex);
        RunAccumulate();
        g.tAcc.end((int)g.frameIndex);

        g.sppAccum += (std::uint64_t)std::max(1, g.settings.sppPerFrame);

        // Denoise
        GLuint hdrForTonemap = g.texAccumHDR;
        if (g.settings.denoiser == Denoiser::AtrousGL)
        {
            g.tDenoise.begin((int)g.frameIndex);
            hdrForTonemap = RunAtrous(g.texAccumHDR);
            g.tDenoise.end((int)g.frameIndex);
        }

        // Tonemap
        g.tTonemap.begin((int)g.frameIndex);

        GLuint selectedHDR = hdrForTonemap;
        const int viewMode = (int)g.settings.view;
        if (g.settings.view == DebugView::Accumulated) selectedHDR = g.texAccumHDR;
        if (g.settings.view == DebugView::Sample)      selectedHDR = g.texSampleHDR;

        RunTonemap(selectedHDR, viewMode);
        g.tTonemap.end((int)g.frameIndex);

        g.stats.internalW = g.internalW;
        g.stats.internalH = g.internalH;
        g.stats.sppAccumulated = g.sppAccum;

        g.stats.msPathTrace = g.tTrace.lastMs;
        g.stats.msAccumulate = g.tAcc.lastMs;
        g.stats.msDenoise = (g.settings.denoiser == Denoiser::AtrousGL) ? g.tDenoise.lastMs : 0.0f;
        g.stats.msTonemap = g.tTonemap.lastMs;

        g.stats.usingMeshScene = g.hasScene;
        g.stats.triCount = g.sceneTriCount;
        g.stats.nodeCount = g.sceneNodeCount;
        g.stats.materialCount = g.sceneMatCount;

        if (g.debugGLFrames > 0) --g.debugGLFrames;
        ++g.frameIndex;
    }
}
