#include "StdInclude.hpp"
#include "CaptureMenu.hpp"

#include "Components/CaptureManager.hpp"
#include "Components/Rendering.hpp"
#include "Configuration/PreferencesConfiguration.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "UI/UIManager.hpp"
#include "Utilities/PathUtils.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    static std::optional<int32_t> displayPassIndex = std::nullopt;

    void CaptureMenu::Show()
    {
        ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoCollapse);

        ImVec2 size = ImGui::GetWindowSize();
        auto& captureManager = Components::CaptureManager::Get();
        auto& captureSettings = captureManager.GetCaptureSettings();
        auto endTick = Mod::GetGameInterface()->GetDemoInfo().endTick;

        ImGui::PushFont(Manager::GetBoldFont());
        ImGui::Text("Output Directory");
        ImGui::PopFont();

        if (ImGui::Button(ICON_FA_FOLDER))
        {
            auto folder = PathUtils::OpenFolderBrowseDialog();
            if (folder.has_value())
            {
                LOG_INFO("Set recording output directory to: {}", folder.value().string());
                PreferencesConfiguration::Get().captureOutputDirectory = folder.value();
                Configuration::Get().Write(true);
            }
        }

        ImGui::SameLine();
        const auto& outputDirectory = PreferencesConfiguration::Get().captureOutputDirectory;
        ImGui::TextWrapped(outputDirectory.string().c_str());

        if (ImGui::Button(ICON_FA_CIRCLE " Capture"))
        {
            if (captureManager.IsCapturing())
                captureManager.StopCapture();
            else
                captureManager.StartCapture();

        }

        if (ImGui::BeginTable("##CaptureTable", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (!captureSettings.multipass)
            {
                ImGui::Text("Filename");
            }
            else
            {
                ImGui::Text("Directory Name");
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint(
                "##FolderName", "Enter recording name...", &captureSettings.name[0],
                captureSettings.name.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData* data) -> int {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        std::string* str = reinterpret_cast<std::string*>(data->UserData);
                        str->resize(data->BufTextLen);
                        data->Buf = &(*str)[0];
                    }
                    return 0;
                },
                &captureSettings.name);

            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Resolution");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##captureMenuResolutionCombo", captureSettings.resolution.ToString().c_str()))
            {
                for (auto resolution : captureManager.GetSupportedResolutions())
                {
                    bool isSelected = captureSettings.resolution == resolution;
                    if (ImGui::Selectable(resolution.ToString().c_str(), captureSettings.resolution == resolution))
                    {
                        captureSettings.resolution = resolution;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Framerate");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##captureMenuFramerateCombo", std::format("{0}", captureSettings.framerate).c_str()))
            {
                for (auto framerate : captureManager.GetSupportedFramerates())
                {
                    bool isSelected = captureSettings.framerate == framerate;
                    if (ImGui::Selectable(std::format("{0}", framerate).c_str(),
                                          captureSettings.framerate == framerate))
                    {
                        captureSettings.framerate = framerate;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

			ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Codec");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##captureMenuVideoCodecCombo",
                                  captureManager.GetVideoCodecLabel(captureSettings.videoCodec.value()).data()))
            {
                for (auto videoCodec = 0; videoCodec < static_cast<std::int32_t>(Components::VideoCodec::Count);
                     videoCodec++)
                {
                    Components::VideoCodec codec = static_cast<Components::VideoCodec>(videoCodec);
                    bool isSelected = captureSettings.videoCodec == codec;
                    if (ImGui::Selectable(captureManager.GetVideoCodecLabel(codec).data(),
                                          captureSettings.videoCodec == codec))
                    {
                        captureSettings.videoCodec = codec;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            std::int32_t tickRange[2] = {
                static_cast<std::int32_t>(captureSettings.startTick),
                static_cast<std::int32_t>(captureSettings.endTick)
            };
			ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
			ImGui::Text("Tick range");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt2("##TickRange", tickRange, 0, endTick);
            captureSettings.startTick = static_cast<std::uint32_t>(tickRange[0]);
            captureSettings.endTick = static_cast<std::uint32_t>(tickRange[1]);

            ImGui::EndTable();
        }

        ImGui::Checkbox("Multi-pass Mode", &captureSettings.multipass);

        if (captureSettings.multipass)
        {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);

            ImGuiWindowFlags childWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
            ImGui::BeginChild("Passes Window", {}, 0, childWindowFlags);
            size = ImGui::GetWindowSize();

            for (auto it = captureSettings.passes.begin(); it != captureSettings.passes.end(); it++)
            {
                auto i = std::distance(captureSettings.passes.begin(), it);
                captureSettings.passes[i].id = static_cast<std::int32_t>(i);

                char passTextID[32] = {0};
                std::snprintf(passTextID, sizeof(passTextID), "##Pass %d id", i + 1);

                char tableID[32] = {0};
                std::snprintf(tableID, sizeof(tableID), "##Table %d", i + 1);

                if (ImGui::BeginTable(tableID, 5, ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(ICON_FA_LAYER_GROUP);
                    char passText[32] = {0};
                    std::snprintf(passText, sizeof(passText), "Pass %d", i + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::InputTextWithHint(
                        passTextID, passText, &captureSettings.passes[i].name[0],
                        captureSettings.passes[i].name.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                        [](ImGuiInputTextCallbackData* data) -> int {
                            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                            {
                                std::string* str = reinterpret_cast<std::string*>(data->UserData);
                                str->resize(data->BufTextLen);
                                data->Buf = &(*str)[0];
                            }
                            return 0;
                        },
                        &captureSettings.passes[i].name);

                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::BeginCombo("##passTypeCombo", magic_enum::enum_name(it->type).data()))
                    {
                        for (auto p = 0; p < (int)Components::PassType::Count; p++)
                        {
                            bool isSelected = it->type == (Components::PassType)p;
                            if (ImGui::Selectable(magic_enum::enum_name((Components::PassType)p).data(), isSelected))
                            {
                                it->type = (Components::PassType)p;
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::TableSetColumnIndex(3);
                    if (displayPassIndex.has_value() && displayPassIndex.value() == i)
                    {
                        if (ImGui::Button(ICON_FA_EYE_SLASH))
                        {
                            displayPassIndex = std::nullopt;
                            Components::Rendering::ResetVisibleElements();
                        }
                        else
                        {
                            Components::Rendering::SetVisibleElements(it->elements);
                        }
                    }
                    else if (!displayPassIndex.has_value())
                    {
                        if (ImGui::Button(ICON_FA_EYE))
                        {
                            displayPassIndex = i;
                            Components::Rendering::SetVisibleElements(it->elements);
                        }
                    }

                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::Button(ICON_FA_MINUS))
                    {
                        displayPassIndex = std::nullopt;
                        Components::Rendering::ResetVisibleElements();
                        captureSettings.passes.erase(it);
                        ImGui::EndTable();
                        break;
                    }

                    ImGui::EndTable();
                }

				ImGui::PushID(passTextID);
				ImGui::PushFont(Manager::GetTQFont());

				if (ImGui::BeginCombo("##elementsCombo", magic_enum::enum_name(it->elements).data()))
				{
					for (auto p = 0; p < (int)Components::VisibleElements::Count; p++)
					{
						bool isSelected = it->elements == (Components::VisibleElements)p;
						if (ImGui::Selectable(magic_enum::enum_name((Components::VisibleElements)p).data(),
											  isSelected))
						{
							it->elements = (Components::VisibleElements)p;
						}

						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				ImGui::Checkbox("ReShade", &it->useReshade);

				ImGui::PopFont();
				ImGui::PopID();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, 0.0f});
            ImVec2 plusButtonSize = {
                size.x - Manager::GetFontSize() * 2.8f,
                Manager::GetFontSize() * 1.1f,
            };
            ImGui::SetCursorPosX((size.x - plusButtonSize.x) / 2.0f);
            if (ImGui::Button(ICON_FA_PLUS, plusButtonSize))
            {
                captureSettings.passes.push_back(
                    {Components::PassType::Default, Components::VisibleElements::Everything});
            }
            ImGui::PopStyleVar();

            ImGui::EndChild();
        }

        ImGui::End();
    }

    const char* CaptureMenu::GetWindowName() noexcept
    {
        return "Capture";
    }

    std::optional<std::int32_t> CaptureMenu::GetDisplayPassIndex()
    {
        return displayPassIndex;
    }
}  // namespace IWXMVM::UI