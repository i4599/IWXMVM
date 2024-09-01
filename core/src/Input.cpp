#include "StdInclude.hpp"
#include "Input.hpp"

#include "Mod.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM
{
    const std::map<ImGuiKey, ImGuiMouseButton> mouseButtonMap = {{ImGuiKey_MouseLeft, ImGuiMouseButton_Left},
                                                                 {ImGuiKey_MouseRight, ImGuiMouseButton_Right},
                                                                 {ImGuiKey_MouseMiddle, ImGuiMouseButton_Middle}};

    bool IsMouseButton(ImGuiKey key)
    {
        return key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY;
    }

    bool Input::KeyDown(ImGuiKey key)
    {
        if (Mod::GetGameInterface()->IsConsoleOpen())
            return false;

        if (IsMouseButton(key))
            return ImGui::IsMouseClicked(mouseButtonMap.at(key));
        return ImGui::IsKeyPressed(key, false);
    }

    bool Input::KeyUp(ImGuiKey key)
    {
        if (Mod::GetGameInterface()->IsConsoleOpen())
            return false;

        if (IsMouseButton(key))
            return ImGui::IsMouseReleased(mouseButtonMap.at(key));
        return ImGui::IsKeyReleased(key);
    }

    bool Input::KeyHeld(ImGuiKey key)
    {
        if (Mod::GetGameInterface()->IsConsoleOpen())
            return false;

        if (IsMouseButton(key))
            return ImGui::IsMouseDown(mouseButtonMap.at(key));
        return ImGui::IsKeyDown(key);
    }

    ImVec2 Input::GetMouseDelta()
    {
        if (Mod::GetGameInterface()->IsConsoleOpen())
            return ImVec2(0, 0);

        ImGuiIO& io = ImGui::GetIO();
        return io.MouseDelta;
    }

    float Input::GetScrollDelta()
    {
        if (Mod::GetGameInterface()->IsConsoleOpen())
            return 0;

        return mouseWheelDelta;
    }

    void Input::UpdateState(ImGuiIO& io)
    {
        mouseWheelDelta = io.MouseWheel;
    }

    float Input::GetDeltaTime()
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.DeltaTime;
    }

    bool Input::BindHeld(Action action)
    {
        return Input::KeyHeld(InputConfiguration::Get().GetBoundKey(action));
    }

    bool Input::BindDown(Action action)
    {
        return Input::KeyDown(InputConfiguration::Get().GetBoundKey(action));
    }
}  // namespace IWXMVM