#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace Notifications
    {
        enum Notification
        {
            NodePlaced,
            NodesDeleted,
        };

        void Send();
        void Render();
    }  // namespace Notifications
}  // namespace IWXMVM::UI