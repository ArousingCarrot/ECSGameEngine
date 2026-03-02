#include "EditorUI.h"
#include "PathTracerGL.h"

#include <imgui/imgui.h>
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor
{
    static bool icontains(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty()) return true;
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char a, char b) { return (char)std::tolower((unsigned char)a) == (char)std::tolower((unsigned char)b); }
        );
        return it != haystack.end();
    }

    static void PushWindowPadding(float x, float y) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(x, y)); }
    static void PopWindowPadding() { ImGui::PopStyleVar(); }

    enum class LayoutPreset : int
    {
        GameDevClassic = 0,
        DebugPerf = 1,
        Minimal = 2,
        CustomSlots = 3,
    };

    static LayoutPreset g_preset = LayoutPreset::GameDevClassic;
    static int g_centerPaneChoice = 0;

    static bool g_showScene = true;
    static bool g_showCode = false;
    static bool g_showHierarchy = true;
    static bool g_showInspector = true;
    static bool g_showContent = true;
    static bool g_showConsole = true;
    static bool g_showProfiler = true;
    static bool g_showPathTracer = true;

    static bool g_showLayout = false;
    static bool g_showPalette = false;

    static const char* kLeftCandidates[] = { "Hierarchy", "Content Browser", "Profiler", "Console" };
    static const char* kRightCandidates[] = { "Inspector", "Profiler", "Content Browser", "Console" };
    static const char* kBottomCandidates[] = { "Console", "Profiler", "Path Tracer", "Diagnostics" };
    static const char* kCenterCandidates[] = { "Scene", "Code" };

    static int g_leftSel = 0;
    static int g_rightSel = 0;
    static int g_bottomSel = 0;

    static bool g_requestRebuild = true;
    static std::uint32_t g_lastLayoutSig = 0;

    static ImTextureID g_sceneTex = (ImTextureID)0;
    static bool g_sceneFlipY = true;

    static SceneViewportInfo g_sceneInfo{};
    static bool g_sceneClickPending = false;

    static float g_sceneBoundsCenter[3] = { 0.0f, 0.0f, 0.0f };
    static float g_sceneBoundsRadius = 1.0f;
    static bool  g_sceneBoundsValid = false;
    static bool  g_sceneBoundsDirty = false;
    static bool  g_sceneFrameRequest = false;

    static char g_paletteFilter[128] = {};

    struct Pane {
        const char* name;
        bool* show;
        void(*drawFn)();
    };

    struct Action {
        std::string name;
        std::function<void()> fn;
    };

    static void RequestRebuild() { g_requestRebuild = true; }

    static void EnsureVisibleByName(const char* name)
    {
        if (!name) return;
        if (std::string_view(name) == "Hierarchy") { g_showHierarchy = true; return; }
        if (std::string_view(name) == "Inspector") { g_showInspector = true; return; }
        if (std::string_view(name) == "Content Browser") { g_showContent = true; return; }
        if (std::string_view(name) == "Console") { g_showConsole = true; return; }
        if (std::string_view(name) == "Profiler") { g_showProfiler = true; return; }
        if (std::string_view(name) == "Path Tracer") { g_showPathTracer = true; return; }
        if (std::string_view(name) == "Scene") { g_showScene = true; return; }
        if (std::string_view(name) == "Code") { g_showCode = true; return; }
        if (std::string_view(name) == "Layout Designer") { g_showLayout = true; return; }
    }

    static void ApplyVisibilityForPreset(LayoutPreset preset)
    {
        g_centerPaneChoice = 0;
        g_showScene = true;
        g_showCode = false;

        if (preset == LayoutPreset::GameDevClassic)
        {
            g_showHierarchy = true;
            g_showContent = true;
            g_showInspector = true;
            g_showConsole = true;
            g_showProfiler = true;
            g_showPathTracer = true;
        }
        else if (preset == LayoutPreset::DebugPerf)
        {
            g_showHierarchy = true;
            g_showContent = false;
            g_showInspector = false;
            g_showConsole = true;
            g_showProfiler = true;
            g_showPathTracer = true;
        }
        else if (preset == LayoutPreset::Minimal)
        {
            g_showHierarchy = false;
            g_showInspector = false;
            g_showContent = false;
            g_showProfiler = false;
            g_showPathTracer = false;
            g_showConsole = true;
        }
        else if (preset == LayoutPreset::CustomSlots)
        {
            EnsureVisibleByName(kLeftCandidates[g_leftSel]);
            EnsureVisibleByName(kRightCandidates[g_rightSel]);
            EnsureVisibleByName(kBottomCandidates[g_bottomSel]);

            g_showScene = true;
            g_showCode = false;
        }
    }

    static std::uint32_t ComputeLayoutSignature()
    {
        std::uint32_t sig = 0;
        sig |= (g_showScene ? 1u : 0u) << 0;
        sig |= (g_showCode ? 1u : 0u) << 1;
        sig |= (g_showHierarchy ? 1u : 0u) << 2;
        sig |= (g_showInspector ? 1u : 0u) << 3;
        sig |= (g_showContent ? 1u : 0u) << 4;
        sig |= (g_showConsole ? 1u : 0u) << 5;
        sig |= (g_showProfiler ? 1u : 0u) << 10;
        sig |= (g_showPathTracer ? 1u : 0u) << 11;
        sig |= (std::uint32_t)g_centerPaneChoice << 12;
        sig |= (std::uint32_t)g_preset << 16;
        sig |= (std::uint32_t)g_leftSel << 20;
        sig |= (std::uint32_t)g_rightSel << 24;
        sig |= (std::uint32_t)g_bottomSel << 28;
        return sig;
    }

    static void DrawScene()
    {
        if (!g_showScene) return;

        g_sceneInfo.clicked = false;

        if (ImGui::Begin("Scene", &g_showScene, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            // Toolbar
            ImGui::Checkbox("Flip Y", &g_sceneFlipY);
            ImGui::SameLine();
            if (ImGui::Button("Frame (R)")) g_sceneFrameRequest = true;
            ImGui::SameLine();
            ImGui::TextDisabled("Tex=%p", (void*)g_sceneTex);

            // the actual viewport.
            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x < 1) avail.x = 1;
            if (avail.y < 1) avail.y = 1;

            ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
            if (scale.x <= 0.0f) scale.x = 1.0f;
            if (scale.y <= 0.0f) scale.y = 1.0f;

            g_sceneInfo.pixelW = (int)(avail.x * scale.x);
            g_sceneInfo.pixelH = (int)(avail.y * scale.y);

            if (g_sceneInfo.pixelW < 1) g_sceneInfo.pixelW = 1;
            if (g_sceneInfo.pixelH < 1) g_sceneInfo.pixelH = 1;

            g_sceneInfo.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            if (g_sceneTex != 0)
            {
                const ImVec2 uv0 = g_sceneFlipY ? ImVec2(0, 1) : ImVec2(0, 0);
                const ImVec2 uv1 = g_sceneFlipY ? ImVec2(1, 0) : ImVec2(1, 1);

                ImGui::Image(g_sceneTex, avail, uv0, uv1);

                g_sceneInfo.hovered = ImGui::IsItemHovered();

                const bool clicked = g_sceneInfo.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                g_sceneInfo.clicked = clicked;
                if (clicked) g_sceneClickPending = true;
            }
            else
            {
                g_sceneInfo.hovered = false;
                ImGui::TextUnformatted("Scene texture not ready.");
            }
        }
        ImGui::End();

        if (!g_showScene && g_centerPaneChoice == 0) // 0 = Scene
        {
            g_centerPaneChoice = 1; // 1 = Code
            g_showCode = true;
            RequestRebuild();
        }
    }


    static void DrawCode()
    {
        if (!g_showCode) return;
        if (ImGui::Begin("Code", &g_showCode))
        {
            ImGui::TextUnformatted("Placeholder.");
            ImGui::TextUnformatted("Next: embed source viewer + file tree + hot-reload.");
        }
        ImGui::End();
    }

    static void DrawHierarchy()
    {
        if (!g_showHierarchy) return;
        if (ImGui::Begin("Hierarchy", &g_showHierarchy))
        {
            ImGui::TextUnformatted("Placeholder.");
        }
        ImGui::End();
    }

    static void DrawInspector()
    {
        if (!g_showInspector) return;
        if (ImGui::Begin("Inspector", &g_showInspector))
        {
            ImGui::TextUnformatted("Placeholder.");
        }
        ImGui::End();
    }

    static void DrawContent()
    {
        if (!g_showContent) return;
        if (ImGui::Begin("Content Browser", &g_showContent))
        {
            ImGui::TextUnformatted("Placeholder.");
        }
        ImGui::End();
    }

    static void DrawConsole()
    {
        if (!g_showConsole) return;
        if (ImGui::Begin("Console", &g_showConsole))
        {
            ImGui::TextUnformatted("Placeholder.");
        }
        ImGui::End();
    }

    static void DrawProfiler()
    {
        if (!g_showProfiler) return;
        if (ImGui::Begin("Profiler", &g_showProfiler))
        {
            const ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("ImGui FPS: %.1f", io.Framerate);
            ImGui::Text("Frame time: %.3f ms", io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
            ImGui::Separator();
            ImGui::TextUnformatted("Next: wire to your metrics registry.");
        }
        ImGui::End();
    }

    static void DrawPathTracer()
    {
        if (!g_showPathTracer) return;
        // PathTracerGL owns the "Path Tracer" window.
        pt::DrawImGuiPanel();
    }

    static void DrawLayoutDesigner()
    {
        if (!g_showLayout) return;

        if (ImGui::Begin("Layout Designer", &g_showLayout))
        {
            int centerSel = g_centerPaneChoice;

            bool changed = false;
            changed |= ImGui::Combo("Center", &centerSel, kCenterCandidates, (int)(sizeof(kCenterCandidates) / sizeof(kCenterCandidates[0])));
            changed |= ImGui::Combo("Left Slot", &g_leftSel, kLeftCandidates, (int)(sizeof(kLeftCandidates) / sizeof(kLeftCandidates[0])));
            changed |= ImGui::Combo("Right Slot", &g_rightSel, kRightCandidates, (int)(sizeof(kRightCandidates) / sizeof(kRightCandidates[0])));
            changed |= ImGui::Combo("Bottom Slot", &g_bottomSel, kBottomCandidates, (int)(sizeof(kBottomCandidates) / sizeof(kBottomCandidates[0])));

            if (changed)
            {
                g_centerPaneChoice = (centerSel == 0) ? 0 : 1;
                g_showScene = (g_centerPaneChoice == 0);
                g_showCode = (g_centerPaneChoice == 1);

                g_preset = LayoutPreset::CustomSlots;
                ApplyVisibilityForPreset(g_preset);
                RequestRebuild();
            }

            ImGui::Separator();

            if (ImGui::Button("Apply Game Dev Classic"))
            {
                g_preset = LayoutPreset::GameDevClassic;
                ApplyVisibilityForPreset(g_preset);
                RequestRebuild();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply Debug/Perf"))
            {
                g_preset = LayoutPreset::DebugPerf;
                ApplyVisibilityForPreset(g_preset);
                RequestRebuild();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply Minimal"))
            {
                g_preset = LayoutPreset::Minimal;
                ApplyVisibilityForPreset(g_preset);
                RequestRebuild();
            }
        }
        ImGui::End();
    }

    static std::vector<Pane> BuildPaneList()
    {
        std::vector<Pane> panes;
        panes.push_back(Pane{ "Scene", &g_showScene, DrawScene });
        panes.push_back(Pane{ "Code",  &g_showCode,  DrawCode });

        panes.push_back(Pane{ "Hierarchy", &g_showHierarchy, DrawHierarchy });
        panes.push_back(Pane{ "Inspector", &g_showInspector, DrawInspector });
        panes.push_back(Pane{ "Content Browser", &g_showContent, DrawContent });
        panes.push_back(Pane{ "Console", &g_showConsole, DrawConsole });
        panes.push_back(Pane{ "Profiler", &g_showProfiler, DrawProfiler });
        panes.push_back(Pane{ "Path Tracer", &g_showPathTracer, DrawPathTracer });
        panes.push_back(Pane{ "Layout Designer", &g_showLayout, DrawLayoutDesigner });
        panes.push_back(Pane{ "Diagnostics", nullptr, []() {} });
        return panes;
    }

    static void TogglePane(bool& flag)
    {
        flag = !flag;
        RequestRebuild();
    }

    static std::vector<Action> BuildActions()
    {
        std::vector<Action> actions;

        actions.push_back({ "Layout: Game Dev Classic", [] { g_preset = LayoutPreset::GameDevClassic; ApplyVisibilityForPreset(g_preset); RequestRebuild(); } });
        actions.push_back({ "Layout: Debug/Perf",       [] { g_preset = LayoutPreset::DebugPerf;      ApplyVisibilityForPreset(g_preset); RequestRebuild(); } });
        actions.push_back({ "Layout: Minimal",          [] { g_preset = LayoutPreset::Minimal;        ApplyVisibilityForPreset(g_preset); RequestRebuild(); } });

        actions.push_back({ "Center Pane: Scene", [] {
            g_centerPaneChoice = 0; g_showScene = true; g_showCode = false; RequestRebuild();
        } });
        actions.push_back({ "Center Pane: Code", [] {
            g_centerPaneChoice = 1; g_showCode = true; g_showScene = false; RequestRebuild();
        } });

        actions.push_back({ "Theme: Dark",    [] { ImGui::StyleColorsDark(); } });
        actions.push_back({ "Theme: Light",   [] { ImGui::StyleColorsLight(); } });
        actions.push_back({ "Theme: Classic", [] { ImGui::StyleColorsClassic(); } });

        auto panes = BuildPaneList();
        for (const auto& p : panes)
        {
            if (p.show)
            {
                actions.push_back({ std::string("Window: Toggle ") + p.name, [show = p.show] { TogglePane(*show); } });
                actions.push_back({ std::string("Window: Focus ") + p.name,  [name = p.name] { ImGui::SetWindowFocus(name); } });
            }
            else
            {
                actions.push_back({ std::string("Window: Focus ") + p.name,  [name = p.name] { ImGui::SetWindowFocus(name); } });
            }
        }

        actions.push_back({ "Open Layout Designer", [] { g_showLayout = true; ImGui::SetWindowFocus("Layout Designer"); } });

        return actions;
    }

    static void DrawCommandPalette()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!g_showPalette && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
            g_showPalette = true;
            g_paletteFilter[0] = 0;
        }

        if (!g_showPalette) return;

        ImGui::SetNextWindowSize(ImVec2(720, 420), ImGuiCond_Appearing);
        if (ImGui::Begin("Command Palette", &g_showPalette, ImGuiWindowFlags_NoDocking))
        {
            ImGui::Separator();

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##palette_filter", g_paletteFilter, sizeof(g_paletteFilter));

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                g_showPalette = false;
            }

            static int selected = 0;

            auto actions = BuildActions();
            std::vector<int> matches;
            matches.reserve(actions.size());

            for (int i = 0; i < (int)actions.size(); ++i)
                if (icontains(actions[i].name, g_paletteFilter))
                    matches.push_back(i);

            if (selected >= (int)matches.size()) selected = (int)matches.size() - 1;
            if (selected < 0) selected = 0;

            if (ImGui::BeginChild("##palette_list", ImVec2(0, 0), true))
            {
                for (int row = 0; row < (int)matches.size(); ++row)
                {
                    const int idx = matches[row];
                    const bool isSel = (row == selected);
                    if (ImGui::Selectable(actions[idx].name.c_str(), isSel))
                        selected = row;
                }
            }
            ImGui::EndChild();

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))   selected = std::max(0, selected - 1);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) selected = std::min((int)matches.size() - 1, selected + 1);

            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            {
                if (!matches.empty())
                {
                    const int idx = matches[selected];
                    actions[idx].fn();
                    g_showPalette = false;
                }
            }
        }
        ImGui::End();
    }

    static void DockIf(bool visible, const char* windowName, ImGuiID dock_id)
    {
        if (!visible) return;
        if (!windowName || dock_id == 0) return;
        ImGui::DockBuilderDockWindow(windowName, dock_id);
    }

    static void ApplyPreset(ImGuiID dockspace_id, LayoutPreset preset)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) return;

        ApplyVisibilityForPreset(preset);

        ImGui::DockBuilderRemoveNodeDockedWindows(dockspace_id, true);
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = 0;
        ImGuiID dock_right = 0;
        ImGuiID dock_bottom = 0;

        bool wantLeft = false;
        bool wantRight = false;
        bool wantBottom = false;

        if (preset == LayoutPreset::GameDevClassic)
        {
            wantLeft = g_showHierarchy || g_showContent;
            wantRight = g_showInspector;
            wantBottom = true;
        }
        else if (preset == LayoutPreset::DebugPerf)
        {
            wantLeft = g_showHierarchy;
            wantRight = g_showProfiler;
            wantBottom = true;
        }
        else if (preset == LayoutPreset::Minimal)
        {
            wantBottom = true;
        }
        else if (preset == LayoutPreset::CustomSlots)
        {
            wantLeft = true;
            wantRight = true;
            wantBottom = true;
        }

        if (wantLeft)
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.22f, &dock_left, &dock_main);

        if (wantRight)
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, &dock_right, &dock_main);

        if (wantBottom)
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.27f, &dock_bottom, &dock_main);

        if (g_centerPaneChoice == 0)
        {
            DockIf(g_showScene, "Scene", dock_main);
            DockIf(g_showCode, "Code", dock_main);
        }
        else
        {
            DockIf(g_showCode, "Code", dock_main);
            DockIf(g_showScene, "Scene", dock_main);
        }

        if (preset == LayoutPreset::GameDevClassic)
        {
            DockIf(g_showHierarchy, "Hierarchy", dock_left ? dock_left : dock_main);
            DockIf(g_showContent, "Content Browser", dock_left ? dock_left : dock_main);
            DockIf(g_showInspector, "Inspector", dock_right ? dock_right : dock_main);

            DockIf(g_showConsole, "Console", dock_bottom ? dock_bottom : dock_main);
            DockIf(g_showProfiler, "Profiler", dock_bottom ? dock_bottom : dock_main);
            ImGui::DockBuilderDockWindow("Path Tracer", dock_bottom ? dock_bottom : dock_main);
            ImGui::DockBuilderDockWindow("Diagnostics", dock_bottom ? dock_bottom : dock_main);
        }
        else if (preset == LayoutPreset::DebugPerf)
        {
            DockIf(g_showHierarchy, "Hierarchy", dock_left ? dock_left : dock_main);
            DockIf(g_showProfiler, "Profiler", dock_right ? dock_right : dock_main);

            DockIf(g_showConsole, "Console", dock_bottom ? dock_bottom : dock_main);
            DockIf(g_showPathTracer, "Path Tracer", dock_bottom ? dock_bottom : dock_main);
            ImGui::DockBuilderDockWindow("Diagnostics", dock_bottom ? dock_bottom : dock_main);
        }
        else if (preset == LayoutPreset::Minimal)
        {
            DockIf(g_showConsole, "Console", dock_bottom ? dock_bottom : dock_main);
            ImGui::DockBuilderDockWindow("Diagnostics", dock_bottom ? dock_bottom : dock_main);
        }
        else if (preset == LayoutPreset::CustomSlots)
        {
            ImGui::DockBuilderDockWindow(kLeftCandidates[g_leftSel], dock_left ? dock_left : dock_main);
            ImGui::DockBuilderDockWindow(kRightCandidates[g_rightSel], dock_right ? dock_right : dock_main);
            ImGui::DockBuilderDockWindow(kBottomCandidates[g_bottomSel], dock_bottom ? dock_bottom : dock_main);

            if (std::string_view(kBottomCandidates[g_bottomSel]) != "Diagnostics")
                ImGui::DockBuilderDockWindow("Diagnostics", dock_bottom ? dock_bottom : dock_main);
        }

        ImGui::DockBuilderFinish(dockspace_id);
        g_lastLayoutSig = ComputeLayoutSignature();
    }

    static void DrawDockHost()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport) return;

        const std::uint32_t sigNow = ComputeLayoutSignature();
        if (sigNow != g_lastLayoutSig)
            g_requestRebuild = true;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        const ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("##DockHost", nullptr, host_flags);

        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);

        if (g_requestRebuild) {
            ApplyPreset(dockspace_id, g_preset);
            g_requestRebuild = false;
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Game Dev Classic")) { g_preset = LayoutPreset::GameDevClassic; ApplyVisibilityForPreset(g_preset); RequestRebuild(); }
                if (ImGui::MenuItem("Debug/Perf")) { g_preset = LayoutPreset::DebugPerf;      ApplyVisibilityForPreset(g_preset); RequestRebuild(); }
                if (ImGui::MenuItem("Minimal")) { g_preset = LayoutPreset::Minimal;        ApplyVisibilityForPreset(g_preset); RequestRebuild(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Layout Designer...")) { g_showLayout = true; }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                auto menuToggle = [&](const char* label, bool& flag)
                    {
                        bool before = flag;
                        ImGui::MenuItem(label, nullptr, &flag);
                        if (before != flag) RequestRebuild();
                    };

                menuToggle("Scene", g_showScene);
                menuToggle("Code", g_showCode);

                ImGui::Separator();

                menuToggle("Hierarchy", g_showHierarchy);
                menuToggle("Inspector", g_showInspector);
                menuToggle("Content Browser", g_showContent);
                menuToggle("Console", g_showConsole);
                menuToggle("Profiler", g_showProfiler);
                menuToggle("Path Tracer", g_showPathTracer);
                menuToggle("Layout Designer", g_showLayout);

                if (ImGui::MenuItem("Focus Diagnostics")) { ImGui::SetWindowFocus("Diagnostics"); }

                ImGui::Separator();

                if (ImGui::MenuItem("Center Pane: Scene"))
                {
                    g_centerPaneChoice = 0;
                    g_showScene = true;
                    g_showCode = false;
                    RequestRebuild();
                }
                if (ImGui::MenuItem("Center Pane: Code"))
                {
                    g_centerPaneChoice = 1;
                    g_showCode = true;
                    g_showScene = false;
                    RequestRebuild();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Theme"))
            {
                if (ImGui::MenuItem("Dark"))    ImGui::StyleColorsDark();
                if (ImGui::MenuItem("Light"))   ImGui::StyleColorsLight();
                if (ImGui::MenuItem("Classic")) ImGui::StyleColorsClassic();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Command Palette (Ctrl+P)")) { g_showPalette = true; }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void DrawEditorUI()
    {
        DrawDockHost();

        DrawScene();
        DrawCode();

        DrawHierarchy();
        DrawInspector();
        DrawContent();
        DrawConsole();
        DrawProfiler();
        DrawPathTracer();
        DrawLayoutDesigner();

        DrawCommandPalette();
    }

    void SetSceneTexture(std::uint64_t textureId)
    {
        g_sceneTex = (ImTextureID)(intptr_t)textureId;
    }

    void SetSceneTexture(std::uint64_t textureId, bool flipY)
    {
        const ImTextureID newTex = (ImTextureID)(intptr_t)textureId;
        if (newTex != g_sceneTex)
        {
            g_sceneTex = newTex;
            g_sceneFlipY = flipY;
        }
        else
        {
            g_sceneTex = newTex;
        }
    }

    const SceneViewportInfo& GetSceneViewportInfo() { return g_sceneInfo; }

    void SetSceneBounds(const float center[3], float radius)
    {
        if (center) {
            g_sceneBoundsCenter[0] = center[0];
            g_sceneBoundsCenter[1] = center[1];
            g_sceneBoundsCenter[2] = center[2];
        }
        g_sceneBoundsRadius = (radius > 1e-6f) ? radius : 1.0f;
        g_sceneBoundsValid = true;
        g_sceneBoundsDirty = true;
    }

    bool GetSceneBounds(float outCenter[3], float& outRadius)
    {
        if (!g_sceneBoundsValid) return false;
        if (outCenter) {
            outCenter[0] = g_sceneBoundsCenter[0];
            outCenter[1] = g_sceneBoundsCenter[1];
            outCenter[2] = g_sceneBoundsCenter[2];
        }
        outRadius = g_sceneBoundsRadius;
        return true;
    }

    bool ConsumeSceneBoundsUpdate(float outCenter[3], float& outRadius)
    {
        if (!g_sceneBoundsDirty) return false;
        g_sceneBoundsDirty = false;
        return GetSceneBounds(outCenter, outRadius);
    }

    void RequestFrame() { g_sceneFrameRequest = true; }

    bool ConsumeFrameRequest()
    {
        if (!g_sceneFrameRequest) return false;
        g_sceneFrameRequest = false;
        return true;
    }


    bool ConsumeSceneClick()
    {
        if (!g_sceneClickPending) return false;
        g_sceneClickPending = false;
        return true;
    }

    bool IsCommandPaletteOpen() { return g_showPalette; }

    bool WantsTextInput() { return g_showPalette || ImGui::GetIO().WantTextInput; }

    CenterPane GetCenterPane()
    {
        return static_cast<CenterPane>(g_centerPaneChoice);
    }
}
