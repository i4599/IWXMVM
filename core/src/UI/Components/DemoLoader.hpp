#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace DemoLoader
    {
        void Initialize();
        void Render();

        bool* GetShowPtr() noexcept;
    }  // namespace DemoLoader
}  // namespace IWXMVM::UI