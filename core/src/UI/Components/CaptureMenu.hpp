#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace CaptureMenu
    {
        void Render();
        std::optional<std::int32_t> GetDisplayPassIndex();
        
        bool* GetShowPtr() noexcept;
    }  // namespace Tabs
}  // namespace IWXMVM::UI