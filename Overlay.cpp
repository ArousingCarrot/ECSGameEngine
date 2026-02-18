#include "Overlay.h"
#include "Chrono.h"
#include "RenderDebugOptions.h"
#include "Stats.h"
#include "Window.h"

#include <imgui/imgui.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace diag {

    static void PrintBytesPretty(const char* label,
        std::uint64_t bTex,
        std::uint64_t bBuf,
        std::uint64_t bMesh,
        std::uint64_t bOther)
    {
        auto line = [&](const char* name, std::uint64_t bytes) {
            double mb = double(bytes) / (1024.0 * 1024.0);
            if (mb >= 0.1) {
                ImGui::Text("%s: %.2f MB", name, mb);
            }
            else {
                double kb = double(bytes) / 1024.0;
                ImGui::Text("%s: %.1f KB", name, kb);
            }
            };

        ImGui::TextUnformatted(label);
        ImGui::Indent();
        line("Tex", bTex);
        line("Buf", bBuf);
        line("Mesh", bMesh);
        line("Other", bOther);
        ImGui::Unindent();
    }

    static const char* SwapIntervalLabel(int interval)
    {
        switch (interval) {
        case 0:  return "0 (Off)";
        case 1:  return "1 (On)";
        case -1: return "-1 (Adaptive)";
        default: return "(Unknown)";
        }
    }

    void Overlay::draw(const MetricsRegistry& mr, const TraceCollector&)
    {
        const auto& fm = mr.currentFrame();
        const auto& memP = mr.processMemory();
        const auto& memE = mr.engineMemory();
        const auto& pct = mr.framePercentiles();

        if (ImGui::Begin("Diagnostics")) {

            // VSync / Swap interval
            static bool vsync = true;
            static int  lastRequested = 1;

            int actualInterval = 0;
            const bool haveInterval = Window::GetSwapInterval(actualInterval);

            if (haveInterval) {
                vsync = (actualInterval != 0);
            }

            if (ImGui::Checkbox("V-Sync", &vsync)) {
                lastRequested = vsync ? 1 : 0;
                (void)Window::SetSwapInterval(lastRequested);

                if (Window::GetSwapInterval(actualInterval)) {
                    vsync = (actualInterval != 0);
                }
            }

            if (haveInterval) {
                ImGui::SameLine();
                ImGui::Text("Actual: %s", SwapIntervalLabel(actualInterval));

                if (actualInterval != lastRequested) {
                    ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f),
                        "Requested %d but driver/OS reports %d",
                        lastRequested, actualInterval);
                }
            }
            else {
                ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f),
                    "Swap interval unknown (SDL_GL_GetSwapInterval failed)");
            }

            ImGui::Separator();

            // Render debug toggles
            auto& opts = GetRenderDebugOptions();

            ImGui::TextUnformatted("Render Debug");
            ImGui::Checkbox("Wireframe", &opts.wireframe);
            ImGui::SameLine();
            ImGui::Checkbox("Textures", &opts.texturesEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Materials", &opts.materialsEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Shader layer", &opts.shaderEnabled);

            ImGui::Checkbox("Disable culling", &opts.disableCulling);

            const char* views[] = { "Lit", "Albedo", "Normal", "UV0", "Depth" };
            int v = (int)opts.view;
            if (ImGui::Combo("Debug view", &v, views, IM_ARRAYSIZE(views)))
                opts.view = (diag::DebugView)v;

            // Frame metrics
            ImGui::Text("FPS: %.1f  | CPU: %.2f ms  GPU: %.2f ms",
                fm.fps, fm.cpu_ms, fm.gpu_ms);
            ImGui::Text("p95: %.2f ms  p99: %.2f ms  (Q1=%.2f, Q3=%.2f)",
                pct.p95, pct.p99, pct.q1, pct.q3);
            if (fm.spike) {
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Spike detected");
            }

            ImGui::Separator();

            // Process / engine memory
            ImGui::Text("Process RSS: %.2f MB  Peak: %.2f MB",
                double(memP.rss_bytes) / (1024.0 * 1024.0),
                double(memP.peak_bytes) / (1024.0 * 1024.0));

            PrintBytesPretty("Engine Memory",
                memE.textures, memE.buffers, memE.meshes, memE.other);

            // Frametime plot (last N frames)
            auto snap_d = mr.frameTimesMs().snapshot();
            if (!snap_d.empty()) {
                std::vector<float> snap_f;
                snap_f.reserve(snap_d.size());
                for (double v : snap_d) {
                    snap_f.push_back(static_cast<float>(v));
                }

                if (ImGui::BeginChild("FrameGraph", ImVec2(0, 150), true)) {
                    ImGui::Text("Frametime (ms), last %zu frames", snap_f.size());
                    ImGui::PlotLines("##ft",
                        snap_f.data(),
                        static_cast<int>(snap_f.size()),
                        0,
                        nullptr,
                        0.0f,
                        50.0f,
                        ImVec2(-1, 100));
                }
                ImGui::EndChild();
            }

            ImGui::Separator();

            // Scope diagnostics
            ImGui::Text("CPU scopes: %zu | GPU scopes: %zu",
                mr.lastCpuScopes().size(),
                mr.lastGpuScopes().size());

            // CPU scope table
            if (ImGui::BeginTable("CPUScopes", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("CPU Scope");
                ImGui::TableSetupColumn("ms");
                ImGui::TableSetupColumn("calls");
                ImGui::TableHeadersRow();

                for (auto& s : mr.lastCpuScopes()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(s.name ? s.name : "(cpu)");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.3f", s.ms);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", s.calls);
                }
                ImGui::EndTable();
            }

            // GPU scope table
            if (ImGui::BeginTable("GPUScopes", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("GPU Scope");
                ImGui::TableSetupColumn("ms");
                ImGui::TableSetupColumn("calls");
                ImGui::TableHeadersRow();

                for (auto& s : mr.lastGpuScopes()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(s.name ? s.name : "(gpu)");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.3f", s.ms);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", s.calls);
                }
                ImGui::EndTable();
            }
        }

        ImGui::End();
    }

}