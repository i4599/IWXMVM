#include "StdInclude.hpp"
#include "Resources.hpp"

namespace IWXMVM::Resources
{
    INCBIN(RUBIK_FONT, "resources/Rubik-Regular.ttf");
    INCBIN(RUBIK_BOLD_FONT, "resources/Rubik-SemiBold.ttf");
    INCBIN(FA_ICONS_FONT, "resources/fa-solid-900.ttf");

    INCBIN(AXIS_MODEL, "resources/axis.obj");
    INCBIN(CAMERA_MODEL, "resources/camera.obj");
    INCBIN(ICOSPHERE_MODEL, "resources/icosphere.obj");
    INCBIN(GIZMO_TRANSLATE_MODEL, "resources/gizmo_translate.obj");
    INCBIN(GIZMO_ROTATE_MODEL, "resources/gizmo_rotate.obj");

    INCBIN(VERTEX_SHADER, "resources/shaders/vertex.hlsl");
    INCBIN(PIXEL_SHADER, "resources/shaders/pixel.hlsl");

    INCBIN(DEPTH_VERTEX_SHADER, "resources/shaders/depth_vs.hlsl");
    INCBIN(DEPTH_PIXEL_SHADER, "resources/shaders/depth_ps.hlsl");
};  // namespace IWXMVM::Resources