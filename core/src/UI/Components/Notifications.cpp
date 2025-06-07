#include "StdInclude.hpp"
#include "Notifications.hpp"

#include "UI/Animations.hpp"
#include "UI/UIManager.hpp"
#include "Resources.hpp"

namespace IWXMVM::UI
{
    struct Notification_Info
    {
        Animation posAnim;
        Animation opacityAnim;
        std::string message;
        Notifications::Notification type;
    };

    static constexpr std::size_t MAX_NOTIFICATIONS = 128;

    static Notification_Info notifs[MAX_NOTIFICATIONS];

    static float notifWidth;
    static float notifHeight;
    static float notifGap;

    static float PosInterp(float a, float b, float t)
    {
        static constexpr float c4 = (2.0f * glm::pi<float>()) / 3.0f;
        t = t == 0.0f ? 0.0f : t == 1.0f ? 1.0f : glm::pow(2.0f, -10.f * t) * glm::sin((t * 10.0f - 0.75f) * c4) + 1.0f;

        return glm::lerp(a, b, t);
    }

    static float OpacityInterp(float a, float b, float t)
    {
        if (t < 0.5f)
        {
            t = 0.0f;
        }
        else
        {
            t = (t - 0.5f) * 2.0f;
        }

        t = -(glm::cos(glm::pi<float>() * t) - 1.0f) / 2.0f;

        return glm::lerp(a, b, t);
    }

    void Notifications::Send(Notification type, const std::string& message)
    {
        std::memmove(notifs + 1, notifs, sizeof(notifs) - sizeof(notifs[0]));
        std::memset(notifs, 0, sizeof(notifs[0]));

        notifWidth = Manager::GetWindowSizeX() / 12.0f;
        notifHeight = notifWidth / 1.4f;
        notifGap = Manager::GetFontSize() * 1.2f;

        notifs[0] = {
            .posAnim = Animation::Create(1.5f, Manager::GetWindowSizeY() + 1.0f,
                                         Manager::GetWindowSizeY() - notifGap - notifHeight, PosInterp),
            .opacityAnim = Animation::Create(4.0f, 1.0f, 0.0f, OpacityInterp),
            .message = message,
            .type = type,
        };
    }

    void Notifications::Show()
    {
        float X = notifGap;

        float lastY = Manager::GetWindowSizeY() + notifHeight + notifGap + 10.0f;

        for (std::size_t i = 0; i < MAX_NOTIFICATIONS; i++)
        {
            if (!notifs[i].posAnim.IsInvalid() && notifs[i].posAnim.ReachedEndVal())
            {
                notifs[i].posAnim.Invalidate();
            }

            if (!notifs[i].opacityAnim.IsInvalid() && notifs[i].opacityAnim.ReachedEndVal())
            {
                notifs[i].opacityAnim.Invalidate();
            }

            if (notifs[i].opacityAnim.IsInvalid())
            {
                break;
            }

            float Y = notifs[i].posAnim.IsInvalid() ? Manager::GetWindowSizeY() - notifGap - notifHeight
                                                    : notifs[i].posAnim.GetVal();
            if (Y > lastY - notifHeight - notifGap)
            {
                Y = lastY - notifHeight - notifGap;
            }

            lastY = Y;

            float opacity = notifs[i].opacityAnim.GetVal();

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity);

            ImVec2 size = {notifWidth, notifHeight};
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);

            ImVec2 pos = {X, Y};
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always);

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoInputs;
            char name[16];
            std::snprintf(name, sizeof(name), "##notif_%d", i);
            if (ImGui::Begin(name, nullptr, flags))
            {
                switch (notifs[i].type)
                {
                    case Notification::Info:
                        ImGui::Text(ICON_FA_CIRCLE_INFO " Info");
                        break;
                    case Notification::Warning:
                        ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION " Warning");
                        break;
                    case Notification::Error:
                        ImGui::Text(ICON_FA_XMARK " Error");
                        break;
                    default:
                        break;
                }

                ImGui::TextWrapped(notifs[i].message.c_str());
            }
            ImGui::End();

            ImGui::PopStyleVar(1);
        }
    }
}  // namespace IWXMVM::UI