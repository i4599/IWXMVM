#include "StdInclude.hpp"
#include "UIManager.hpp"

#include "Components/CameraManager.hpp"
#include "Components/Playback.hpp"
#include "Configuration/InputConfiguration.hpp"
#include "UIComponent.hpp"
#include "Utilities/HookManager.hpp"
#include "Mod.hpp"
#include "Events.hpp"
#include "Resources.hpp"
#include "Input.hpp"
#include "Utilities/MathUtils.hpp"
#include "UI/Animations.hpp"
#include "UI/Components/CameraMenu.hpp"
#include "UI/Components/CaptureMenu.hpp"
#include "UI/Components/ControlBar.hpp"
#include "UI/Components/DemoLoader.hpp"
#include "UI/Components/KeyframeEditor.hpp"
#include "UI/Components/Notifications.hpp"
#include "UI/Components/PlayerAnimationUI.hpp"
#include "UI/Components/Tabs.hpp"
#include "UI/Components/VisualsMenu.hpp"
#include "UI/TaskbarProgress.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    INCBIN_EXTERN(NOTOSANS_FONT);
    INCBIN_EXTERN(NOTOSANS_BOLD_FONT);
    INCBIN_EXTERN(FA_ICONS_FONT);

    static bool isInitialized = false;
    static bool hideOverlay = false;
    static bool overlayHiddenThisFrame = false;
    static bool dockBuilderInitialized = false;
    static ImVec2 windowSize;
    static ImVec2 simulatedMouse;
    static ImVec2 mousePosWhenHid;
    static ImVec2 mouseDelta;

    static ImFont* font;
    static float fontSize;

    // Sizes of these are a multiple of fontSize and can be accessed through the getter functions
    static ImFont* boldFont;
    static ImFont* tqFont;
    static ImFont* hFont;

    static WNDPROC originalGameWndProc;

    static Animation opacityAnim;

    static void SetGameCursorVisibility(bool show)
    {
        if (!show)
        {
            Mod::GetGameInterface()->SetMouseMode(Types::MouseMode::Capture);
        }
        else
        {
            Mod::GetGameInterface()->SetMouseMode(Types::MouseMode::Passthrough);
        }
    }

    static HRESULT ImGuiWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        WPARAM newLParam = lParam;

        if (hideOverlay && Mod::GetGameInterface()->GetGameState() == Types::GameState::InDemo && GetFocus() == hWnd)
        {
            switch (uMsg)
            {
                case WM_MOUSEMOVE:
                case WM_NCMOUSEMOVE:
                {
                    ImVec2 mousePos = {static_cast<float>(GET_X_LPARAM(lParam)),
                                       static_cast<float>(GET_Y_LPARAM(lParam))};
                    ImVec2 middle = {windowSize.x / 2.0f, windowSize.y / 2.0f};

                    middle.y -= 12.0f;

                    if (!overlayHiddenThisFrame)
                    {
                        mouseDelta.x = mousePos.x - middle.x;
                        mouseDelta.y = mousePos.y - middle.y;
                    }
                    else
                    {
                        mouseDelta = {};
                    }

                    simulatedMouse.x += mouseDelta.x * 0.5f;
                    simulatedMouse.y += mouseDelta.y * 0.5f;

                    newLParam = MAKELPARAM(static_cast<LONG>(simulatedMouse.x), static_cast<LONG>(simulatedMouse.y));
                }
            }
        }

        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, newLParam))
        {
            return 1L;
        }

        if (!hideOverlay)
        {
            ShowCursor(true);

            if (ImGui::GetIO().WantCaptureKeyboard)
            {
                switch (uMsg)
                {
                    case WM_CHAR:
                    case WM_HOTKEY:
                    case WM_KEYDOWN:
                    case WM_KEYUP:
                    case WM_SYSKEYDOWN:
                    case WM_SYSKEYUP:
                        return 1L;
                }
            }
        }

        return CallWindowProc(originalGameWndProc, hWnd, uMsg, wParam, lParam);
    }

    static float OpacityInterp(float a, float b, float t)
    {
        return std::lerp(a, b, t);
    }

    static void SetImGuiStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.DisabledAlpha = 1.0f;
        style.WindowPadding = {};
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 2.0f;
        style.ChildRounding = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.PopupRounding = 0.0f;
        style.PopupBorderSize = 0.0f;
        style.FramePadding = {Manager::GetFontSize() * 0.2f, 0.0f};
        style.FrameRounding = 0.0f;
        style.FrameBorderSize = 2.0f;
        style.ItemSpacing = {};
        style.ItemInnerSpacing = {Manager::GetFontSize() * 0.25f, 0.0f};
        style.CellPadding = {};
        style.IndentSpacing = 0.0f;
        style.ColumnsMinSpacing = 0.0f;
        style.ScrollbarSize = 12.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabMinSize = 12.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.DisplayWindowPadding = {100.0f, 100.0f};

        /*
        style.Colors[ImGuiCol_Text] = {1.00f, 1.00f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_TextDisabled] = {0.50f, 0.50f, 0.50f, 1.00f};
        style.Colors[ImGuiCol_WindowBg] = {0.04f, 0.04f, 0.04f, 1.00f};
        style.Colors[ImGuiCol_ChildBg] = {0.00f, 0.00f, 0.00f, 0.00f};
        style.Colors[ImGuiCol_PopupBg] = {0.19f, 0.19f, 0.19f, 0.92f};
        style.Colors[ImGuiCol_Border] = {0.22f, 0.22f, 0.22f, 1.0f};
        style.Colors[ImGuiCol_BorderShadow] = {0.00f, 0.00f, 0.00f, 0.24f};
        style.Colors[ImGuiCol_FrameBg] = {0.12f, 0.12f, 0.12f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.19f, 0.19f, 0.19f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive] = {0.20f, 0.22f, 0.23f, 1.00f};
        style.Colors[ImGuiCol_TitleBg] = {0.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive] = {0.1f, 0.1f, 0.1f, 1.00f};
        style.Colors[ImGuiCol_TitleBgCollapsed] = {0.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg] = {0.14f, 0.14f, 0.14f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg] = {0.05f, 0.05f, 0.05f, 0.54f};
        style.Colors[ImGuiCol_ScrollbarGrab] = {0.34f, 0.34f, 0.34f, 0.54f};
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = {0.40f, 0.40f, 0.40f, 0.54f};
        style.Colors[ImGuiCol_ScrollbarGrabActive] = {0.56f, 0.56f, 0.56f, 0.54f};
        style.Colors[ImGuiCol_CheckMark] = {0.33f, 0.67f, 0.86f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab] = {0.34f, 0.34f, 0.34f, 0.54f};
        style.Colors[ImGuiCol_SliderGrabActive] = {0.56f, 0.56f, 0.56f, 0.54f};
        style.Colors[ImGuiCol_Button] = {0.05f, 0.05f, 0.05f, 0.54f};
        style.Colors[ImGuiCol_ButtonHovered] = {0.19f, 0.19f, 0.19f, 0.54f};
        style.Colors[ImGuiCol_ButtonActive] = {0.20f, 0.22f, 0.23f, 1.00f};
        style.Colors[ImGuiCol_Header] = {0.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_HeaderHovered] = {0.3f, 0.3f, 0.3f, 1.00f};
        style.Colors[ImGuiCol_HeaderActive] = {0.20f, 0.22f, 0.23f, 0.33f};
        style.Colors[ImGuiCol_Separator] = {0.28f, 0.28f, 0.28f, 1.00f};
        style.Colors[ImGuiCol_SeparatorHovered] = {0.8f, 0.8f, 0.8f, 0.29f};
        style.Colors[ImGuiCol_SeparatorActive] = {0.40f, 0.44f, 0.47f, 1.00f};
        style.Colors[ImGuiCol_ResizeGrip] = {0.28f, 0.28f, 0.28f, 0.29f};
        style.Colors[ImGuiCol_ResizeGripHovered] = {0.44f, 0.44f, 0.44f, 0.29f};
        style.Colors[ImGuiCol_ResizeGripActive] = {0.40f, 0.44f, 0.47f, 1.00f};
        style.Colors[ImGuiCol_Tab] = {0.00f, 0.00f, 0.00f, 0.52f};
        style.Colors[ImGuiCol_TabHovered] = {0.14f, 0.14f, 0.14f, 1.00f};
        style.Colors[ImGuiCol_TabActive] = {0.20f, 0.20f, 0.20f, 0.36f};
        style.Colors[ImGuiCol_TabUnfocused] = {0.00f, 0.00f, 0.00f, 0.52f};
        style.Colors[ImGuiCol_TabUnfocusedActive] = {0.14f, 0.14f, 0.14f, 1.00f};
        style.Colors[ImGuiCol_PlotLines] = {1.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_PlotLinesHovered] = {1.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_PlotHistogram] = {1.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_PlotHistogramHovered] = {1.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_TableHeaderBg] = {0.00f, 0.00f, 0.00f, 0.52f};
        style.Colors[ImGuiCol_TableBorderStrong] = {0.00f, 0.00f, 0.00f, 0.52f};
        style.Colors[ImGuiCol_TableBorderLight] = {0.28f, 0.28f, 0.28f, 0.29f};
        style.Colors[ImGuiCol_TableRowBg] = {0.00f, 0.00f, 0.00f, 0.00f};
        style.Colors[ImGuiCol_TableRowBgAlt] = {1.00f, 1.00f, 1.00f, 0.06f};
        style.Colors[ImGuiCol_TextSelectedBg] = {0.20f, 0.22f, 0.23f, 1.00f};
        style.Colors[ImGuiCol_DragDropTarget] = {0.33f, 0.67f, 0.86f, 1.00f};
        style.Colors[ImGuiCol_NavHighlight] = {1.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_NavWindowingHighlight] = {1.00f, 0.00f, 0.00f, 0.70f};
        style.Colors[ImGuiCol_NavWindowingDimBg] = {1.00f, 0.00f, 0.00f, 0.20f};
        style.Colors[ImGuiCol_ModalWindowDimBg] = {1.00f, 0.00f, 0.00f, 0.35f};
        */
    }

    void Manager::Initialize(IDirect3DDevice9* device, HWND hwnd, ImVec2 size)
    {
        windowSize = size;
        fontSize = std::floor(windowSize.y / 45.0f);

        try
        {
            LOG_DEBUG("Initializing UI Manager...");

            LOG_DEBUG("Creating ImGui context");
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGui::Spectrum::StyleColorsSpectrum();
            ImGui::GetStyle().GrabMinSize = Manager::GetFontSize() / 4.0f;

            LOG_DEBUG("Initializing ImGui_ImplWin32 with HWND {0:x}", (std::uintptr_t)hwnd);
            ImGui_ImplWin32_Init(hwnd);

            LOG_DEBUG("Initializing ImGui_ImplDX9 with D3D9 Device {0:x}", (std::uintptr_t)device);
            ImGui_ImplDX9_Init(device);

            // TODO: byte size is game dependent
            LOG_DEBUG("Hooking WndProc at {0:x}", Mod::GetGameInterface()->GetWndProc());
            originalGameWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (std::uintptr_t)ImGuiWndProc);

            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = NULL;
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.Fonts->ClearFonts();
            io.ConfigInputTrickleEventQueue = false;

            auto RegisterFont = [&](const char* name, const uint8_t* data, size_t size, float fontSize) -> ImFont* {
                ImFontConfig fontConfig;
                fontConfig.FontDataOwnedByAtlas = false;
                fontConfig.SizePixels = fontSize;
                assert(strlen(name) < sizeof(fontConfig.Name));
                std::strncpy(fontConfig.Name, name, sizeof(fontConfig.Name));

                ImFont* loadedFont = io.Fonts->AddFontFromMemoryTTF((void*)data, size, fontSize, &fontConfig);
                if (!loadedFont)
                {
                    LOG_ERROR("Failed to register font {}", name);
                    return nullptr;
                }

                static constexpr ImWchar iconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
                ImFontConfig iconsConfig;
                iconsConfig.FontDataOwnedByAtlas = false;
                iconsConfig.MergeMode = true;
                iconsConfig.PixelSnapH = true;
                iconsConfig.GlyphMinAdvanceX = GetIconsFontSize();
                // iconsConfig.SizePixels = GetIconsFontSize();
                io.Fonts->AddFontFromMemoryTTF((void*)FA_ICONS_FONT_data, FA_ICONS_FONT_size, GetIconsFontSize(),
                                               &iconsConfig, iconsRanges);

                LOG_DEBUG("Registered font {0} with size {1}", name, fontSize);

                return loadedFont;
            };

            font = RegisterFont("NotoSans Regular", NOTOSANS_FONT_data, NOTOSANS_FONT_size, GetFontSize());
            boldFont =
                RegisterFont("NotoSans Bold", NOTOSANS_BOLD_FONT_data, NOTOSANS_BOLD_FONT_size, GetBoldFontSize());
            tqFont = RegisterFont("NotoSans Regular - TQ", NOTOSANS_FONT_data, NOTOSANS_FONT_size, GetTQFontSize());
            hFont = RegisterFont("NotoSans Regular - H", NOTOSANS_FONT_data, NOTOSANS_FONT_size, GetHFontSize());

            SetGameCursorVisibility(hideOverlay);

            Components::CaptureManager::Get().Initialize();
            TaskbarProgress::Initialize(hwnd);

            DemoLoader::Initialize();
            KeyframeEditor::Initialize();

            isInitialized = true;
            dockBuilderInitialized = false;

            LOG_INFO("Initialized UI");
        }
        catch (...)
        {
            throw std::runtime_error("Failed to initialize UI");
            // TODO: panic
        }
    }

    void Manager::Shutdown()
    {
        LOG_DEBUG("Shutting down UI Manager");

        isInitialized = false;

        SetWindowLongPtr(D3D9::FindWindowHandle(), GWLP_WNDPROC, (LONG_PTR)originalGameWndProc);
        SetGameCursorVisibility(true);

        TaskbarProgress::Shutdown();

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void Manager::Frame()
    {
        try
        {
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            Input::UpdateState(ImGui::GetIO());

            overlayHiddenThisFrame = false;

            if (Input::KeyDown(ImGuiKey_F1))
            {
                hideOverlay = !hideOverlay;
                SetGameCursorVisibility(hideOverlay);

                if (hideOverlay)
                {
                    overlayHiddenThisFrame = true;
                    mousePosWhenHid = ImGui::GetMousePos();
                    simulatedMouse = mousePosWhenHid;

                    if (opacityAnim.IsInvalid())
                    {
                        opacityAnim = Animation::Create(0.1f, 1.0f, 0.0f, OpacityInterp);
                    }
                    else
                    {
                        opacityAnim.GoForward();
                    }
                }
                else
                {
                    // ImGui::TeleportMousePos(mousePosWhenHid);
                    opacityAnim.GoBackward();
                }
            }

            if (!opacityAnim.IsInvalid())
            {
                ImGui::GetStyle().Alpha = opacityAnim.GetVal();
            }

            // Render interface
            {
                // Set up a couple variables for the layout of the whole UI
                const float screenPadding = Manager::GetFontSize() * 0.5f;

                const ImVec2 timelineSize = {Manager::GetWindowSizeX() / 2.4f, Manager::GetBoldFontSize() * 2.0f};
                const ImVec2 timelinePos = {(Manager::GetWindowSizeX() - timelineSize.x) / 2.0f,
                                            Manager::GetWindowSizeY() - timelineSize.y - screenPadding};

                static ImVec2 mainWndSize = {(Manager::GetWindowSizeX() - timelineSize.x) / 2.0f - screenPadding * 2.0f,
                                             Manager::GetWindowSizeY() / 1.4f};
                static ImVec2 mainWndPos = {timelinePos.x + timelineSize.x + screenPadding,
                                            Manager::GetWindowSizeY() - mainWndSize.y - screenPadding};
                const float minMainWndSizeY = Manager::GetFontSize() * 2.0f;
                const float maxMainWndSizeY = Manager::GetWindowSizeY() - screenPadding * 2.0f;

                static bool showSettings = true;
                static bool showKeyframeEditor = false;

                // This ensures that the timeline, keyframe editor, and tabs only get rendered inside of a demo
				if (Mod::GetGameInterface()->GetGameState() == Types::GameState::InDemo)
				{
					const auto demoInfo = Mod::GetGameInterface()->GetDemoInfo();
					const auto currentTick = Components::Playback::GetTimelineTick();

					if (currentTick < demoInfo.endTick)
					{
						ControlBar::Show(timelineSize, timelinePos);

						Tabs::RenderSettingsToggle(timelineSize, timelinePos, &showSettings);
						Tabs::RenderKeyframeEditorToggle(timelineSize, timelinePos, &showKeyframeEditor);

						KeyframeEditor::Show(&showKeyframeEditor);
					}
				}

                if (Mod::GetGameInterface()->GetGameState() != Types::GameState::InDemo || showSettings)
                {
                    // Dock builder
                    ImGuiID dockspaceID = ImGui::GetID("Main Dockspace");
                    if (!dockBuilderInitialized)
                    {
                        dockBuilderInitialized = true;

                        ImGui::DockBuilderRemoveNode(dockspaceID);
                        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);

                        ImGui::DockBuilderDockWindow(VisualsMenu::GetWindowName(), dockspaceID);
                        ImGui::DockBuilderDockWindow(CaptureMenu::GetWindowName(), dockspaceID);
                        ImGui::DockBuilderDockWindow(CameraMenu::GetWindowName(), dockspaceID);
                        ImGui::DockBuilderDockWindow(PlayerAnimationUI::GetWindowName(), dockspaceID);
                        ImGui::DockBuilderDockWindow(DemoLoader::GetWindowName(), dockspaceID);

                        ImGui::DockBuilderFinish(dockspaceID);
                    }

                    // Main window
                    {
                        // Resizing area & logic
						const float resizeThickness = Manager::GetFontSize() * 0.4f;
                        {
                            const ImVec2 resizeTopLeft = mainWndPos;
                            const ImVec2 resizeBotRight = {resizeTopLeft.x + mainWndSize.x,
                                                           resizeTopLeft.y + resizeThickness};
                            const ImVec2 mousePos = ImGui::GetMousePos();

                            static bool currentlyResizing = false;
                            bool hoveringResize = false;

                            if (mousePos.x >= resizeTopLeft.x && mousePos.x <= resizeBotRight.x &&
                                mousePos.y >= resizeTopLeft.y && mousePos.y <= resizeBotRight.y)
                            {
                                hoveringResize = true;
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                {
                                    currentlyResizing = true;
                                }
                            }

                            if (currentlyResizing && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
                            {
                                currentlyResizing = false;
                            }

                            if (currentlyResizing)
                            {
                                const float deltaY = -ImGui::GetIO().MouseDelta.y;
                                mainWndSize.y += deltaY;
                                mainWndPos.y -= deltaY;

                                mainWndSize.y = glm::clamp(mainWndSize.y, minMainWndSizeY, maxMainWndSizeY);
                                mainWndPos.y = glm::clamp(mainWndPos.y, screenPadding,
                                                          screenPadding + maxMainWndSizeY - minMainWndSizeY);
                            }

                            if (hoveringResize || currentlyResizing)
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                            }
                        }

                        ImGuiWindowFlags mainWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                                           ImGuiWindowFlags_NoDocking;
                        ImGui::SetNextWindowSize(mainWndSize, ImGuiCond_Always);
                        ImGui::SetNextWindowPos(mainWndPos, ImGuiCond_Always);
                        ImGui::Begin("Main window", nullptr, mainWindowFlags);

                        // Draw a line at the top as a visual cue that you can resize the window
					    {
                            ImDrawList* draw = ImGui::GetWindowDrawList();

                            const ImVec2 p = mainWndPos;
                            const float width = mainWndSize.x;

                            const ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);
                            const float thickness = 2.0f;
                            draw->AddLine(ImVec2(p.x, p.y + thickness * 0.5f),
                                          ImVec2(p.x + width, p.y + thickness * 0.5f), col, thickness);
                        }

                        ImGui::SetCursorPos({});
                        ImGui::Dummy({mainWndSize.x, resizeThickness});

                        ImGui::DockSpace(dockspaceID);
                        ImGui::End();
                    }

                    VisualsMenu::Show();
                    CaptureMenu::Show();
                    CameraMenu::Show();
                    PlayerAnimationUI::Show();
                    DemoLoader::Show();
                }

				Notifications::Show();
            }

            Events::Invoke(EventType::OnFrame);

            Animation::UpdateAll(ImGui::GetIO().DeltaTime);

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
        catch (std::exception& e)
        {
            LOG_CRITICAL("An exception occurred while rendering the IWXMVM user interface: {0}", e.what());
            __debugbreak();
        }
        catch (...)
        {
            LOG_CRITICAL("An error occurred while rendering the IWXMVM user interface");

            // TODO: panic function
            MessageBox(NULL, "An error occurred while rendering the IWXMVM user interface", "FATAL ERROR", MB_OK);
        }
    }

    bool Manager::IsInitialized() noexcept
    {
        return isInitialized;
    }

    bool Manager::IsHidden() noexcept
    {
        return hideOverlay;
    }

    ImVec2 Manager::GetWindowPos() noexcept
    {
        RECT rect = {};
        GetWindowRect(D3D9::FindWindowHandle(), &rect);
        return {static_cast<float>(rect.left), static_cast<float>(rect.top)};
    }

    ImVec2 Manager::GetWindowSize() noexcept
    {
        return windowSize;
    }

    float Manager::GetWindowSizeX() noexcept
    {
        return windowSize.x;
    }

    float Manager::GetWindowSizeY() noexcept
    {
        return windowSize.y;
    }

    ImFont* Manager::GetFont() noexcept
    {
        return font;
    }

    float Manager::GetFontSize() noexcept
    {
        return fontSize;
    }

    ImFont* Manager::GetBoldFont() noexcept
    {
        return boldFont;
    }

    float Manager::GetBoldFontSize() noexcept
    {
        return fontSize * 1.5f;
    }

    ImFont* Manager::GetTQFont() noexcept
    {
        return tqFont;
    }

    float Manager::GetTQFontSize() noexcept
    {
        return fontSize * 3.0f / 4.0f;
    }

    ImFont* Manager::GetHFont() noexcept
    {
        return hFont;
    }

    float Manager::GetHFontSize() noexcept
    {
        return fontSize / 2.0f;
    }

    float Manager::GetIconsFontSize() noexcept
    {
        return fontSize * 2.0f / 3.0f;
    }
}  // namespace IWXMVM::UI
