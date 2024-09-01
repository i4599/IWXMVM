#pragma once
#include "StdInclude.hpp"

#include "UIComponent.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace IWXMVM::UI
{
    namespace Component
    {
        enum Component
        {
            Background = 0,
            MenuBar,
            GameView,
            PrimaryTabs,
            DemoLoader,
            CameraMenu,
            VisualsMenu,
            CaptureMenu,
            ControlBar,
            DebugPanel,
            KeyframeEditor,
            ControlsMenu,
            Preferences,
            PlayerAnimation,
            Readme,
            Credits,

            Count,
        };
    }

    namespace Manager
    {
        void Initialize(IDirect3DDevice9* device, HWND hwnd, ImVec2 size);
        void Shutdown();
        void Frame();

        bool IsInitialized() noexcept;
        bool IsBlurInitialized() noexcept;
        bool IsHidden() noexcept;

        ImVec2 GetWindowPos() noexcept;
        ImVec2 GetWindowSize() noexcept;
        float GetWindowSizeX() noexcept;
        float GetWindowSizeY() noexcept;

        ImFont* GetFont() noexcept;
        float GetFontSize() noexcept;

        ImFont* GetBoldFont() noexcept;
        float GetBoldFontSize() noexcept;

        // Same as main font but 3/4 of original size
        ImFont* GetTQFont() noexcept;
        float GetTQFontSize() noexcept;

        // Same as main font but 1/2 of original size
        ImFont* GetHFont() noexcept;
        float GetHFontSize() noexcept;

        float GetIconsFontSize() noexcept;
    };
}  // namespace IWXMVM::UI