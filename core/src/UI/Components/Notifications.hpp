#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace Notifications
    {
        enum class Notification
        {
            Info,
            Warning,
            Error,
        };

        void Send(Notification type, const std::string& message);
        void Render();
    }  // namespace Notifications
}  // namespace IWXMVM::UI