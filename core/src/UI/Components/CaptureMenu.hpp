#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace CaptureMenu
    {
        void Show();
        const char* GetWindowName() noexcept;
        std::optional<std::int32_t> GetDisplayPassIndex();
    }  // namespace Tabs
}  // namespace IWXMVM::UI