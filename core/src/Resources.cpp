#include "StdInclude.hpp"
#include "Resources.hpp"

namespace IWXMVM::Resources
{
    INCBIN(NOTOSANS_FONT, "resources/NotoSans-Regular.ttf");
    INCBIN(NOTOSANS_BOLD_FONT, "resources/NotoSans-Bold.ttf");
    INCBIN(FA_ICONS_FONT, "resources/fa-solid-900.ttf");

    INCBIN(AXIS_MODEL, "resources/axis.obj");
    INCBIN(CAMERA_MODEL, "resources/camera.obj");
    INCBIN(ICOSPHERE_MODEL, "resources/icosphere.obj");
    INCBIN(GIZMO_TRANSLATE_MODEL, "resources/gizmo_translate.obj");
    INCBIN(GIZMO_ROTATE_MODEL, "resources/gizmo_rotate.obj");

    INCBIN(VERTEX_SHADER, "resources/shaders/vertex.hlsl");
    INCBIN(PIXEL_SHADER, "resources/shaders/pixel.hlsl");

    INCBIN(KAWASE_DOWN_SHADER, "resources/shaders/kawase_down_ps.hlsl");
    INCBIN(KAWASE_UP_SHADER, "resources/shaders/kawase_up_ps.hlsl");
    INCBIN(BLUR_VSHADER, "resources/shaders/blur_vs.hlsl");

    INCBIN(DEPTH_VERTEX_SHADER, "resources/shaders/depth_vs.hlsl");
    INCBIN(DEPTH_PIXEL_SHADER, "resources/shaders/depth_ps.hlsl");
};  // namespace IWXMVM::Resources