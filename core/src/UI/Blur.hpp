#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace Blur
    {
        bool CreateResources();
        void DestroyResources();
        void Capture();
        void RenderToWindow(ImVec2 size, ImVec2 pos, float scroll = 0.0f);
        IDirect3DTexture9* GetTexture() noexcept;
    }
}  // namespace IWXMVM::UI