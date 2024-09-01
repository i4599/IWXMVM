#include "StdInclude.hpp"
#include "FreeCamera.hpp"

#include "Input.hpp"
#include "Events.hpp"
#include "Resources.hpp"
#include "UI/UIManager.hpp"
#include "Components/CameraManager.hpp"
#include "Configuration/PreferencesConfiguration.hpp"

namespace IWXMVM::Components
{
    constexpr float PROPERTY_DISPLAY_TIME = 4.0f;
    float fovDisplayTimer = 0.0f;
    float rotationDisplayTimer = 0.0f;

    int GetAlphaForTimer(float timer)
    {
        return (int)(timer / PROPERTY_DISPLAY_TIME * 2 * 255.0f);
    }

    void FreeCamera::Initialize()
    {
    }

    void FreeCamera::Update()
    {
        if (!UI::Manager::IsHidden())
            return;

        HandleFreecamInput(this->position, this->rotation, this->fov, this->GetForwardVector(), this->GetRightVector());
    }

    void FreeCamera::HandleFreecamInput(glm::vec3& position, glm::vec3& rotation, float& fov, glm::vec3 forward, glm::vec3 right)
    {
        auto& preferences = PreferencesConfiguration::Get();

        constexpr float SCROLL_LOWER_BOUNDARY = -0.001f;
        constexpr float SCROLL_UPPER_BOUNDARY = 0.001f;
        const float SMOOTHING_FACTOR = glm::clamp(1.0f - 15.0f * Input::GetDeltaTime(), 0.0f, 1.0f);

        static auto scrollDelta = 0.0f;

        auto speedModifier = Input::BindHeld(Action::FreeCameraFaster) ? 3.0f : 1.0f;
        speedModifier *= Input::BindHeld(Action::FreeCameraSlower) ? 0.1f : 1.0f;

        const auto cameraBaseSpeed = Input::GetDeltaTime() * preferences.freecamSpeed;
        const auto cameraMovementSpeed = cameraBaseSpeed * speedModifier;
        const auto cameraHeightSpeed = cameraMovementSpeed;

        if (Input::BindHeld(Action::FreeCameraForward))
        {
            position += forward * cameraMovementSpeed;
        }

        if (Input::BindHeld(Action::FreeCameraBackward))
        {
            position -= forward * cameraMovementSpeed;
        }

        if (Input::BindHeld(Action::FreeCameraLeft))
        {
            position += right * cameraMovementSpeed;
        }

        if (Input::BindHeld(Action::FreeCameraRight))
        {
            position -= right * cameraMovementSpeed;
        }

        if (Input::KeyHeld(ImGuiKey_LeftAlt))
        {
            static float scrollDeltaRot = 0.0f;
            scrollDeltaRot -= Input::GetScrollDelta();

            if (scrollDeltaRot < SCROLL_LOWER_BOUNDARY || scrollDeltaRot > SCROLL_UPPER_BOUNDARY)
            {
                rotation[2] += scrollDeltaRot * Input::GetDeltaTime() * 32.0f;
                scrollDeltaRot *= SMOOTHING_FACTOR;
                rotationDisplayTimer = PROPERTY_DISPLAY_TIME;
            }
            else if (scrollDeltaRot != 0.0)
            {
                scrollDeltaRot = 0.0;
            }
        }
        else
        {
            scrollDelta -= Input::GetScrollDelta();

            if (scrollDelta < SCROLL_LOWER_BOUNDARY || scrollDelta > SCROLL_UPPER_BOUNDARY)
            {
                fov += scrollDelta * Input::GetDeltaTime() * 32.0f;
                fov = glm::clamp(fov, 1.0f, 179.0f);
                scrollDelta *= SMOOTHING_FACTOR;
                fovDisplayTimer = PROPERTY_DISPLAY_TIME;
            }
            else if (scrollDelta != 0.0)
            {
                scrollDelta = 0.0;
            }
        }

        if (Input::BindHeld(Action::FreeCameraReset))
        {
            rotation = {};
            fov = 90.0f;
        }

        rotation[0] += Input::GetMouseDelta()[1] * preferences.freecamMouseSpeed * fov / 90;
        rotation[1] -= Input::GetMouseDelta()[0] * preferences.freecamMouseSpeed * fov / 90;
        
        rotation[0] = glm::clamp(rotation[0], -89.9f, 89.9f);

        if (Input::BindHeld(Action::FreeCameraUp))
            position[2] += cameraHeightSpeed;

        if (Input::BindHeld(Action::FreeCameraDown))
            position[2] -= cameraHeightSpeed;
    }
}  // namespace IWXMVM::Components
