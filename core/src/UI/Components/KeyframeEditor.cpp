#include "StdInclude.hpp"
#include "KeyframeEditor.hpp"

#include "Components/Playback.hpp"
#include "Components/Rewinding.hpp"
#include "Input.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "Types/Dvar.hpp"
#include "Types/KeyframeableProperty.hpp"
#include "UI/Blur.hpp"
#include "UI/UIManager.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    static std::int32_t displayStartTick, displayEndTick;

    static std::map<Types::KeyframeableProperty, bool> propertyVisible;
    static std::map<Types::KeyframeablePropertyType, std::vector<ImVec2>> verticalZoomRanges;

    static std::optional<int32_t> selectedKeyframeId = std::nullopt;
    static std::optional<int32_t> selectedKeyframeValueIndex = std::nullopt;

    static std::tuple<int32_t, int32_t> GetDisplayTickRange()
    {
        return {displayStartTick, displayEndTick};
    }

    static ImVec2 GetPositionForKeyframe(const ImRect rect, const Types::Keyframe& keyframe, const uint32_t startTick,
                                         const uint32_t endTick, const ImVec2 valueBoundaries = ImVec2(-1, 1),
                                         int32_t valueIndex = 0)
    {
        const auto barHeight = rect.Max.y - rect.Min.y;
        const auto barLength = rect.Max.x - rect.Min.x;
        const auto tickPercentage =
            static_cast<float>(keyframe.tick - startTick) / static_cast<float>(endTick - startTick);
        const auto x = rect.Min.x + tickPercentage * barLength;
        const auto valuePercentage =
            1 - (keyframe.value.GetByIndex(valueIndex) - valueBoundaries.x) / (valueBoundaries.y - valueBoundaries.x);
        const auto y = rect.Min.y + valuePercentage * barHeight;
        return ImVec2(x, y);
    };

    static uint32_t GetTickForPosition(const float x, const ImRect rect, const uint32_t startTick,
                                       const uint32_t endTick)
    {
        const auto barLength = rect.Max.x - rect.Min.x;
        const auto tickPercentage = (x - rect.Min.x) / barLength;
        return static_cast<uint32_t>(startTick + tickPercentage * (endTick - startTick));
    }

    static std::tuple<uint32_t, Types::KeyframeValue> GetKeyframeForPosition(
        const ImVec2 position, const ImRect rect, const uint32_t startTick, const uint32_t endTick,
        const ImVec2 valueBoundaries = ImVec2(-1, 1))
    {
        const auto tick = GetTickForPosition(position.x, rect, startTick, endTick);
        const auto barHeight = rect.Max.y - rect.Min.y;
        const auto valuePercentage = (position.y - rect.Min.y) / barHeight;
        const auto value = valueBoundaries.x + (1 - valuePercentage) * (valueBoundaries.y - valueBoundaries.x);
        return std::make_tuple(tick, value);
    };

    static void HandleVerticalPan(const ImVec2 rectMin, const ImVec2 rectMax,
                                  const Types::KeyframeableProperty& property, std::optional<int32_t> valueIndex)
    {
        static int32_t heldValueIndex = -1;

        if (valueIndex.has_value() && Input::KeyDown(ImGuiKey_MouseLeft))
        {
            heldValueIndex = valueIndex.value();
        }

        if (valueIndex.has_value() && Input::KeyUp(ImGuiKey_MouseLeft))
        {
            heldValueIndex = -1;
        }

        auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            if (valueIndex.has_value() && heldValueIndex == valueIndex.value())
            {
                auto currentRange = verticalZoomRanges[property.type][valueIndex.value()];
                auto scaledDelta = delta / (rectMax.y - rectMin.y);
                scaledDelta *= currentRange.y - currentRange.x;
                verticalZoomRanges[property.type][valueIndex.value()].x += scaledDelta.y;
                verticalZoomRanges[property.type][valueIndex.value()].y += scaledDelta.y;
            }

            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }

    static void HandleTimelineZoomInteractions(const ImVec2 rectMin, const ImVec2 rectMax,
                                               const Types::KeyframeableProperty& property,
                                               std::optional<int32_t> valueIndex)
    {
        constexpr auto ZOOM_MULTIPLIER = 2000;
        constexpr auto MOVE_MULTIPLIER = 1;

        constexpr int32_t MINIMUM_ZOOM = 500;

        const auto minTick = 0;
        const auto maxTick = (int32_t)Mod::GetGameInterface()->GetDemoInfo().endTick;

        auto scrollDelta = Input::GetScrollDelta() * Input::GetDeltaTime();

        // center zoom section around mouse cursor if zooming in
        const auto barLength = rectMax.x - rectMin.x;
        const auto mousePercentage = (ImGui::GetMousePos().x - rectMin.x) / barLength;

        scrollDelta = scrollDelta * 100 * ZOOM_MULTIPLIER * maxTick / 50000;
        displayStartTick += static_cast<uint32_t>(scrollDelta * mousePercentage);
        displayStartTick = glm::clamp(displayStartTick, minTick, displayEndTick - MINIMUM_ZOOM);
        displayEndTick -= static_cast<uint32_t>(scrollDelta * (1.0f - mousePercentage));
        displayEndTick = glm::clamp(displayEndTick, displayStartTick + MINIMUM_ZOOM, maxTick);

        auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.1f);

        if (glm::abs(delta.x) > glm::abs(delta.y) * 1.25f)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                auto zoomWindowSizeBefore = displayEndTick - displayStartTick;

                auto scaledDelta = delta / (rectMax.x - rectMin.x);
                scaledDelta *= static_cast<float>(displayEndTick - displayStartTick);

                displayStartTick = glm::max(displayStartTick - (int32_t)(scaledDelta.x * MOVE_MULTIPLIER), minTick);
                displayEndTick = glm::min(displayEndTick - (int32_t)(scaledDelta.x * MOVE_MULTIPLIER), maxTick);

                auto zoomWindowSizeAfter = displayEndTick - displayStartTick;

                if (zoomWindowSizeAfter != zoomWindowSizeBefore)
                {
                    if (displayStartTick == minTick)
                        displayEndTick = displayStartTick + zoomWindowSizeBefore;
                    else
                        displayStartTick = displayEndTick - zoomWindowSizeBefore;
                }

                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
        }
        else
        {
            HandleVerticalPan(rectMin, rectMax, property, valueIndex);
        }
    }

    static bool DrawKeyframeSliderInternal(const Types::KeyframeableProperty& property, uint32_t* currentTick,
                                           uint32_t displayStartTick, uint32_t displayEndTick,
                                           std::vector<Types::Keyframe>& keyframes, uint32_t demoLength,
                                           std::optional<uint32_t> frozenTick)
    {
        using namespace ImGui;

        ImGuiWindow* window = GetCurrentWindow();

        const char* label = property.name.data();

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = CalcItemWidth();

        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const ImRect frame_bb(window->DC.CursorPos,
                              window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));

        ItemSize(frame_bb, style.FramePadding.y);
        ItemAdd(frame_bb, id, &frame_bb, 0);

        // Draw frame
        const ImU32 frame_col = GetColorU32(ImGuiCol_FrameBg);
        RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

        // Draw timeline indicator
        float halfNeedleWidth = 2;

        auto t =
            static_cast<float>(*currentTick - displayStartTick) / static_cast<float>(displayEndTick - displayStartTick);
        ImRect grab_bb = ImRect(frame_bb.Min.x + t * w - halfNeedleWidth, frame_bb.Min.y,
                                frame_bb.Min.x + t * w + halfNeedleWidth, frame_bb.Max.y);

        if (grab_bb.Max.x < frame_bb.Max.x)
            window->DrawList->AddRectFilled(
                grab_bb.Min, grab_bb.Max,
                GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

        bool isAnyKeyframeHovered = false;

        const auto textSize = CalcTextSize(ICON_FA_DIAMOND);
        for (auto it = keyframes.begin(); it != keyframes.end();)
        {
            auto& k = *it;

            const auto positionX = GetPositionForKeyframe(frame_bb, k, displayStartTick, displayEndTick).x;

            const auto barHeight = frame_bb.Max.y - frame_bb.Min.y;
            ImRect text_bb(ImVec2(positionX - textSize.x / 2, frame_bb.Min.y + (barHeight - textSize.y) / 2),
                           ImVec2(positionX + textSize.x / 2, frame_bb.Max.y - (barHeight - textSize.y) / 2));
            const bool hovered = ItemHoverable(text_bb, id, g.LastItemData.ItemFlags);

            isAnyKeyframeHovered |= hovered;

            if (hovered && IsMouseClicked(ImGuiMouseButton_Left) && selectedKeyframeId == std::nullopt &&
                selectedKeyframeValueIndex == std::nullopt)
            {
                selectedKeyframeId = k.id;
                selectedKeyframeValueIndex = std::nullopt;
            }

            const ImU32 col =
                GetColorU32(hovered || selectedKeyframeId == k.id ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            window->DrawList->AddText(text_bb.Min, col, ICON_FA_DIAMOND);

            if (hovered && IsMouseClicked(ImGuiMouseButton_Right))
            {
                Components::KeyframeManager::Get().RemoveKeyframe(property, it);
                break;
            }

            if (selectedKeyframeId == k.id)
            {
                auto [tick, _] = GetKeyframeForPosition(GetMousePos(), frame_bb, displayStartTick, displayEndTick);
                if (!Components::KeyframeManager::Get().IsKeyframeTickBeingModified(k))
                {
                    Components::KeyframeManager::Get().BeginModifyingKeyframeTick(k);
                }
                k.tick = tick;
                Components::KeyframeManager::Get().SortAndSaveKeyframes(keyframes);

                if (IsMouseReleased(ImGuiMouseButton_Left))
                {
                    selectedKeyframeId = std::nullopt;
                    selectedKeyframeValueIndex = std::nullopt;

                    if (Components::KeyframeManager::Get().IsKeyframeTickBeingModified(k))
                    {
                        if (Components::KeyframeManager::Get().IsKeyframeValueBeingModified(k))
                        {
                            Components::KeyframeManager::Get().EndModifyingKeyframeTickAndValue(property, k);
                        }
                        else
                        {
                            Components::KeyframeManager::Get().EndModifyingKeyframeTick(property, k);
                        }
                    }
                }
            }

            ++it;
        }

        const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
        if (hovered && !isAnyKeyframeHovered && !selectedKeyframeId.has_value())
        {
            HandleTimelineZoomInteractions(frame_bb.Min, frame_bb.Max, property, std::nullopt);
        }

        if (hovered && !isAnyKeyframeHovered && IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            auto [tick, _] = GetKeyframeForPosition(GetMousePos(), frame_bb, displayStartTick, displayEndTick);

            if (std::find_if(keyframes.begin(), keyframes.end(), [tick](const auto& k) { return k.tick == tick; }) ==
                keyframes.end())
            {
                keyframes.emplace_back(property, tick, Components::KeyframeManager::Get().Interpolate(property, tick));
                Components::KeyframeManager::Get().SortAndSaveKeyframes(keyframes);
            }
        }

        return hovered;
    }

    static bool DrawKeyframeSlider(const Types::KeyframeableProperty& property)
    {
        auto demoInfo = Mod::GetGameInterface()->GetDemoInfo();
        auto currentTick = Components::Playback::GetTimelineTick();
        auto [displayStartTick, displayEndTick] = GetDisplayTickRange();

        return DrawKeyframeSliderInternal(property, &currentTick, displayStartTick, displayEndTick,
                                          Components::KeyframeManager::Get().GetKeyframes(property), demoInfo.endTick,
                                          Components::Playback::GetFrozenTick());
    }

    ImVec2 KeyframeEditor::Render()
    {
        if (Mod::GetGameInterface()->GetGameState() != Types::GameState::InDemo)
        {
            return {};
        }

        const auto demoInfo = Mod::GetGameInterface()->GetDemoInfo();
        const auto currentTick = Components::Playback::GetTimelineTick();

        if (currentTick >= demoInfo.endTick)
        {
            return {};
        }

        float width = Manager::GetWindowSizeX() / 2.4f;
        float height = width / 15.0f;
        ImVec2 size = {width, height};
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        float X = (Manager::GetWindowSizeX() - width) / 2.0f;
        float Y = Manager::GetWindowSizeY() - height - height / 7.0f;
        ImVec2 pos = {X, Y};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Keyframes", NULL, flags))
        {
            Blur::RenderToWindow(size, pos);

            float barWidth = size.x * (6.0f / 7.0f);
            ImVec2 barPos = {(size.x - barWidth) / 2.0f, (size.y - Manager::GetFontSize()) / 2.0f};
            ImGui::SetNextItemWidth(barWidth);
            ImGui::SetCursorPos(barPos);

            displayStartTick = 0;
            displayEndTick = Mod::GetGameInterface()->GetDemoInfo().endTick;

            DrawKeyframeSlider({Types::KeyframeablePropertyType::CampathCamera, "Campath",
                                Types::KeyframeValueType::CameraData, -360.0f, 360.0f});
        }
        ImGui::End();

        return pos;
    }
}  // namespace IWXMVM::UI