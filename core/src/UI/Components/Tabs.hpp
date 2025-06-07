#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    namespace Tabs
    {
        // The keyframe editor toggle is to the left of the settings toggle
        void RenderKeyframeEditorToggle(ImVec2 timelineSize, ImVec2 timelinePos, bool* show);

        // The settings toggle is to the left of the timeline
        void RenderSettingsToggle(ImVec2 timelineSize, ImVec2 timelinePos, bool* show);
    }  // namespace Tabs
}  // namespace IWXMVM::UI